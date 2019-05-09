/*
 * Copyright 2019 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

import QtQuick 2.8
import QtQuick.Layouts 1.1

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.extras 2.0 as PlasmaExtras

import org.kde.kcoreaddons 1.0 as KCoreAddons

import org.kde.notificationmanager 1.0 as NotificationManager

GridLayout {
    id: detailsGrid

    property QtObject jobDetails

    columns: 2
    rowSpacing: Math.round(units.smallSpacing / 2)
    columnSpacing: units.smallSpacing

    // once you use Layout.column/Layout.row *all* of the items in the Layout have to use them
    Repeater {
        model: [1, 2]

        PlasmaExtras.DescriptiveLabel {
            Layout.column: 0
            Layout.row: index
            Layout.alignment: Qt.AlignTop | Qt.AlignRight
            text: jobDetails["descriptionLabel" + modelData] && jobDetails["descriptionValue" + modelData]
                ? i18nc("Row description, e.g. Source", "%1:", jobDetails["descriptionLabel" + modelData]) : ""
            font: theme.smallestFont
            textFormat: Text.PlainText
            visible: text !== ""
        }
    }

    Repeater {
        model: [1, 2]

        PlasmaExtras.DescriptiveLabel {
            id: descriptionValueLabel
            Layout.column: 1
            Layout.row: index
            Layout.fillWidth: true
            font: theme.smallestFont
            elide: Text.ElideMiddle
            textFormat: Text.PlainText
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 5
            visible: text !== ""

            Component.onCompleted: bindText()
            function bindText() {
                text = Qt.binding(function() {
                    return jobDetails["descriptionLabel" + modelData] && jobDetails["descriptionValue" + modelData]
                            ? jobDetails["descriptionValue" + modelData] : "";
                });
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onPressed: {
                    // break binding so it doesn't update while the menu is opened
                    descriptionValueLabel.text = descriptionValueLabel.text;
                    descriptionValueMenu.open(mouse.x, mouse.y)
                }
            }

            EditContextMenu {
                id: descriptionValueMenu
                target: descriptionValueLabel
                // defer re-binding until after the "Copy" action in the menu has triggered
                onClosed: Qt.callLater(descriptionValueLabel.bindText)
            }
        }
    }

    Repeater {
        model: ["Bytes", "Files", "Directories"]

        PlasmaExtras.DescriptiveLabel {
            Layout.column: 1
            Layout.row: 2 + index
            Layout.fillWidth: true
            text: {
                var processed = jobDetails["processed" + modelData];
                var total = jobDetails["total" + modelData];

                if (processed > 0 || total > 1) {
                    if (processed > 0 && total > 0 && processed <= total) {
                        switch(modelData) {
                        case "Bytes":
                            return i18nc("How many bytes have been copied", "%2 of %1",
                                KCoreAddons.Format.formatByteSize(total),
                                KCoreAddons.Format.formatByteSize(processed))
                        case "Files":
                            return i18ncp("How many files have been copied", "%2 of %1 file", "%2 of %1 files",
                                          total, processed);
                        case "Directories":
                            return i18ncp("How many dirs have been copied", "%2 of %1 folder", "%2 of %1 folders",
                                         total, processed);
                        }
                    } else {
                        switch(modelData) {
                        case "Bytes":
                            return KCoreAddons.Format.formatByteSize(processed || total)
                        case "Files":
                            return i18np("%1 file", "%1 files", (processed || total));
                        case "Directories":
                            return i18np("%1 folder", "%1 folders", (processed || total));
                        }
                    }
                }

                return "";
            }
            font: theme.smallestFont
            textFormat: Text.PlainText
            visible: text !== ""
        }
    }

    PlasmaExtras.DescriptiveLabel {
        Layout.column: 1
        Layout.row: 2 + 3
        Layout.fillWidth: true
        text: jobDetails.speed > 0 ? i18nc("Bytes per second", "%1/s",
                                           KCoreAddons.Format.formatByteSize(jobDetails.speed)) : ""
        font: theme.smallestFont
        textFormat: Text.PlainText
        visible: text !== ""
    }
}
