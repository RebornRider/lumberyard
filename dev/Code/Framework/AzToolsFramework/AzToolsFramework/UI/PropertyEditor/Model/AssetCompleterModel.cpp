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

#include <UI/PropertyEditor/Model/AssetCompleterModel.h>
#include <UI/PropertyEditor/Model/AssetCompleterModel.moc>
#include <AzToolsFramework/AssetBrowser/AssetBrowserModel.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserEntry.h>
#include <AzToolsFramework/AssetBrowser/Search/Filter.h>
#include <AzFramework/StringFunc/StringFunc.h>

namespace AzToolsFramework
{
    AssetCompleterModel::AssetCompleterModel(QObject* parent) 
        : QAbstractTableModel(parent)
    {
    }

    AssetCompleterModel::~AssetCompleterModel()
    {
    }

    void AssetCompleterModel::SetFilter(AZ::Data::AssetType filterType)
    {
        beginResetModel();

        m_assets.clear();

        AssetBrowserModel* assetBrowserModel = nullptr;
        AssetBrowserComponentRequestBus::BroadcastResult(assetBrowserModel, &AssetBrowserComponentRequests::GetAssetBrowserModel);

        AZ_Error("AssetCompleterModel", (assetBrowserModel != nullptr), "Unable to setup Source Model, asset browser model was not returned correctly.");

        AssetBrowserFilterModel* assetBrowserFilterModel = aznew AssetBrowserFilterModel(this);
        assetBrowserFilterModel->setSourceModel(assetBrowserModel);

        AssetTypeFilter* typeFilter = new AssetTypeFilter();
        typeFilter->SetAssetType(filterType);
        typeFilter->SetFilterPropagation(AssetBrowserEntryFilter::PropagateDirection::Down);

        assetBrowserFilterModel->sort(0, Qt::DescendingOrder);
        assetBrowserFilterModel->SetFilter(FilterConstType(typeFilter));

        FetchResources(assetBrowserFilterModel, QModelIndex());

        endResetModel();

        emit dataChanged(this->index(0, 0), this->index(rowCount(), columnCount()));
    }

    void AssetCompleterModel::SearchStringHighlight(QString searchString)
    {
        m_highlightString = searchString;
    }

    int AssetCompleterModel::rowCount(const QModelIndex&) const
    {
        return static_cast<int>(m_assets.size());
    }

    int AssetCompleterModel::columnCount(const QModelIndex&) const
    {
        /*  Model has 2 columns:
        *   - Column 0 returns the Asset Name (used for autocompletion)
        *   - Column 1 returns the Asset Name with highlight of current Search String (used in suggestions popup)
        */
        return 2;
    }

    QVariant AssetCompleterModel::data(const QModelIndex& index, int role) const
    {
        switch(role) 
        {
            case Qt::DisplayRole:
            {
                if (index.column() == 0 || m_highlightString.isEmpty())
                {
                    return QString(m_assets[index.row()].m_displayName.data());
                }
                                
                QString displayString = QString(m_assets[index.row()].m_displayName.data());
                return  "<span style=\"color: #fff;\">" + 
                            displayString.replace(m_highlightString, "<span style=\"background-color: #498FE1\">" + m_highlightString + "</span>", Qt::CaseInsensitive) + 
                        "</span>";
            }

            case Qt::ToolTipRole:
            {
                return QString(m_assets[index.row()].m_path.data());
            }

            default:
            {
                return QVariant();
            }
        }
    }

    AssetBrowserEntry* AssetCompleterModel::GetAssetEntry(QModelIndex index) const
    {
        if (!index.isValid())
        {
            AZ_Error("AssetCompleterModel", false, "Invalid Source Index provided to GetAssetEntry.");
            return nullptr;
        }

        return static_cast<AssetBrowserEntry*>(index.internalPointer());
    }

    void AssetCompleterModel::FetchResources(AssetBrowserFilterModel* filter, QModelIndex index)
    {
        int rows = filter->rowCount(index);
        if (rows == 0)
        {
            if (index != QModelIndex()) {
                AZ_Error("AssetCompleterModel", false, "No children detected in FetchResources()");
            }
            return;
        }

        for (int i = 0; i < rows; ++i)
        {
            QModelIndex childIndex = filter->index(i, 0, index);
            AssetBrowserEntry* childEntry = GetAssetEntry(filter->mapToSource(childIndex));

            if (childEntry->GetEntryType() == AssetBrowserEntry::AssetEntryType::Product)
            {
                ProductAssetBrowserEntry* productEntry = static_cast<ProductAssetBrowserEntry*>(childEntry);
                AZStd::string assetName;
                AzFramework::StringFunc::Path::GetFileName(productEntry->GetFullPath().c_str(), assetName);
                m_assets.push_back({
                    assetName, productEntry->GetFullPath(), productEntry->GetAssetId()
                });
            }

            if (childEntry->GetChildCount() > 0) 
            {
                FetchResources(filter, childIndex);
            }
        }
    }

    Qt::ItemFlags AssetCompleterModel::flags(const QModelIndex &index) const
    {
        if (!index.isValid())
        {
            return Qt::NoItemFlags;
        }

        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    const AZStd::string_view AssetCompleterModel::GetNameFromIndex(const QModelIndex& index) 
    {
        if (!index.isValid())
        {
            return "";
        }

        return m_assets[index.row()].m_displayName;
    }


    const AZ::Data::AssetId AssetCompleterModel::GetAssetIdFromIndex(const QModelIndex& index) 
    {
        if (!index.isValid())
        {
            return AZ::Data::AssetId();
        }

        return m_assets[index.row()].m_assetId;
    }
}