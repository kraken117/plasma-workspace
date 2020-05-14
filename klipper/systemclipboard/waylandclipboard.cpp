/*
   Copyright (C) 2020 David Edmundson <davidedmundson@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the Lesser GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "waylandclipboard.h"

#include <QFile>
#include <QFutureWatcher>
#include <QPointer>
#include <QDebug>
#include <QGuiApplication>

#include <QtWaylandClient/QWaylandClientExtension>

#include <qpa/qplatformnativeinterface.h>

#include <unistd.h>
#include <memory>

#include "qwayland-wlr-data-control-unstable-v1.h"

class DataControlDeviceManager : public QWaylandClientExtensionTemplate<DataControlDeviceManager>
        , public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
public:
    DataControlDeviceManager()
        : QWaylandClientExtensionTemplate<DataControlDeviceManager>(1)
    {
    }

    ~DataControlDeviceManager() {
        destroy();
    }
};

class DataControlOffer: public QMimeData, public QtWayland::zwlr_data_control_offer_v1
{
    Q_OBJECT
public:
    DataControlOffer(struct ::zwlr_data_control_offer_v1 *id):
        QtWayland::zwlr_data_control_offer_v1(id)
    {
    }

    ~DataControlOffer() {
        destroy();
    }

    QStringList formats() const override
    {
        return m_receivedFormats;
    }

    bool hasFormat(const QString &format) const override {
         return m_receivedFormats.contains(format);
    }
protected:
    void zwlr_data_control_offer_v1_offer(const QString &mime_type) override {
        m_receivedFormats << mime_type;
    }

    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;
private:
    QStringList m_receivedFormats;
};


QVariant DataControlOffer::retrieveData(const QString &mimeType, QVariant::Type type) const
{
    if (!hasFormat(mimeType)) {
        return QVariant();
    }
    Q_UNUSED(type);

    int pipeFds[2];
    if (pipe(pipeFds) != 0){
        return QVariant();
    }

    auto t = const_cast<DataControlOffer*>(this);
    t->receive(mimeType, pipeFds[1]);

    close(pipeFds[1]);

    /*
     * Ideally we need to introduce a non-blocking QMimeData object
     * Or a non-blocking constructor to QMimeData with the mimetypes that are relevant
     *
     * However this isn't actually any worse than X.
     */

    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    auto display = static_cast<struct ::wl_display*>(native->nativeResourceForIntegration("wl_display"));
    wl_display_flush(display);

    QFile readPipe;
    if (readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
        QByteArray data;
        data = readPipe.readAll();
        close(pipeFds[0]);
        return data;
    }
    return QVariant();
}

class DataControlSource: public QObject, public QtWayland::zwlr_data_control_source_v1
{
    Q_OBJECT
public:
    DataControlSource(struct ::zwlr_data_control_source_v1 *id, QMimeData *mimeData);
    DataControlSource();
    ~DataControlSource() {
        destroy();
    }

Q_SIGNALS:
    void cancelled();

protected:
    void zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_source_v1_cancelled() override;
private:
    QMimeData *m_mimeData;
};

DataControlSource::DataControlSource(struct ::zwlr_data_control_source_v1 *id, QMimeData *mimeData)
    : QtWayland::zwlr_data_control_source_v1(id)
    , m_mimeData(mimeData)
{
    for (const QString &format: mimeData->formats()) {
        offer(format);
    }
}

void DataControlSource::zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd)
{
    QFile c;
    if (c.open(fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
        c.write(m_mimeData->data(mime_type));
        c.close();
    }
}

void DataControlSource::zwlr_data_control_source_v1_cancelled()
{
    Q_EMIT cancelled();
}

class DataControlDevice : public QObject, public QtWayland::zwlr_data_control_device_v1
{
    Q_OBJECT
public:
    DataControlDevice(struct ::zwlr_data_control_device_v1 *id)
        : QtWayland::zwlr_data_control_device_v1(id)
    {}

    ~DataControlDevice() {
        destroy();
    }

    void setSelection(std::unique_ptr<DataControlSource> selection);
    DataControlOffer *receivedSelection() {
        return m_receivedSelection.get();
    }

Q_SIGNALS:
    void receivedSelectionChanged();
protected:
    void zwlr_data_control_device_v1_data_offer(struct ::zwlr_data_control_offer_v1 *id) override {
        new DataControlOffer(id);
        // this will become memory managed when we retrieve the selection event
        // a compositor calling data_offer without doing that would be a bug
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override {
        if(!id ) {
            m_receivedSelection.reset();
        } else {
            auto deriv = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
            auto offer = dynamic_cast<DataControlOffer*>(deriv); //dynamic because of the dual inheritance
            m_receivedSelection.reset(offer);
        }
        emit receivedSelectionChanged();
    }

private:
    std::unique_ptr<DataControlSource> m_selection; // selection set locally
    std::unique_ptr<DataControlOffer> m_receivedSelection; // latest selection set from externally to here
};


void DataControlDevice::setSelection(std::unique_ptr<DataControlSource> selection)
{
    m_selection = std::move(selection);
    connect(m_selection.get(), &DataControlSource::cancelled, this, [this]() {
        m_selection.reset();
    });
    set_selection(m_selection->object());
}

WaylandClipboard::WaylandClipboard(QObject *parent)
    : SystemClipboard(parent)
    , m_manager(new DataControlDeviceManager)
{
    connect(m_manager.data(), &DataControlDeviceManager::activeChanged, this, [this]() {
        if (m_manager->isActive()) {

            QPlatformNativeInterface *native = qApp->platformNativeInterface();
            if (!native) {
                return;
            }
            auto seat = static_cast<struct ::wl_seat*>(native->nativeResourceForIntegration("wl_seat"));
            if (!seat) {
                return;
            }

            m_device.reset(new DataControlDevice(m_manager->get_data_device(seat)));

            connect(m_device.get(), &DataControlDevice::receivedSelectionChanged, this, [this]() {
                    emit changed(QClipboard::Clipboard);
            });
        } else {
            m_device.reset();
        }
    });
}

void WaylandClipboard::setMimeData(QMimeData *mime, QClipboard::Mode mode)
{
    if (!m_device) {
        return;
    }
    auto source = std::unique_ptr<DataControlSource>(new DataControlSource(m_manager->create_data_source(), mime));
    if (mode == QClipboard::Selection) {
        m_device->setSelection(std::move(source));
    }
}

void WaylandClipboard::clear(QClipboard::Mode mode)
{
    if (!m_device) {
        return;
    }
    if (mode == QClipboard::Clipboard) {
        m_device->set_selection(nullptr);
    } else if (mode == QClipboard::Selection) {
        m_device->set_primary_selection(nullptr);
    }
}
const QMimeData* WaylandClipboard::mimeData(QClipboard::Mode mode) const
{
    if (!m_device) {
        return nullptr;
    }
    if (mode == QClipboard::Clipboard) {
        return m_device->receivedSelection();
    }
    return nullptr;
}

#include "waylandclipboard.moc"
