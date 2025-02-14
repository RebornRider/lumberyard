/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#pragma once

#include <AzQtComponents/AzQtComponentsAPI.h>
#include <QWidget>
#include <QSet>

class QUndoStack;
class QVBoxLayout;
class QMargins;

namespace AzQtComponents
{
    class Palette;
    class PaletteCard;
    namespace Internal
    {
        class ColorController;
    }

    class AZ_QT_COMPONENTS_API PaletteCardCollection
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit PaletteCardCollection(Internal::ColorController* colorController, QUndoStack* undoStack, QWidget* parent = nullptr);
        ~PaletteCardCollection() override;

        QSharedPointer<PaletteCard> makeCard(QSharedPointer<Palette> palette, const QString& title);
        void addCard(QSharedPointer<PaletteCard> card);
        void removeCard(QSharedPointer<PaletteCard> card);
        bool containsCard(QSharedPointer<PaletteCard> card) const;

        void setSwatchSize(const QSize& size);

        int count() const;
        QSharedPointer<PaletteCard> paletteCard(int index) const;
        int indexOf(const QSharedPointer<PaletteCard>& card) const;

        void moveUp(QSharedPointer<PaletteCard>& card);
        bool canMoveUp(QSharedPointer<PaletteCard>& card);

        void moveDown(QSharedPointer<PaletteCard>& card);
        bool canMoveDown(QSharedPointer<PaletteCard>& card);

        void setCardContentMargins(const QMargins& margins);

        public Q_SLOTS:
        void setGammaEnabled(bool enabled);
        void setGamma(qreal gamma);

    Q_SIGNALS:
        void removePaletteClicked(QSharedPointer<PaletteCard> card);
        void savePaletteClicked(QSharedPointer<PaletteCard> card);
        void savePaletteAsClicked(QSharedPointer<PaletteCard> card);
        void paletteCountChanged();

    private:
        QString uniquePaletteName(QSharedPointer<PaletteCard> card, const QString& name) const;
        void paletteCardDestroyed(QObject* obj);

        Internal::ColorController* m_colorController;
        QUndoStack* m_undoStack;

        QVBoxLayout* m_layout;
        QVector<QSharedPointer<PaletteCard>> m_paletteCards;
        QSize m_swatchSize;
        bool m_gammaEnabled = false;
        qreal m_gamma = 1.0;

        QSet<QObject*> m_registeredPaletteCards;
    };
} // namespace AzQtComponents
