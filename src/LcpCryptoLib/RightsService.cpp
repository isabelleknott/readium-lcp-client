//
//  Created by Artem Brazhnikov on 11/15.
//  Copyright © 2015 Mantano. All rights reserved.
//
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//  Licensed under Gnu Affero General Public License Version 3 (provided, notwithstanding this notice,
//  Readium Foundation reserves the right to license this material under a different separate license,
//  and if you have done so, the terms of that separate license control and the following references
//  to GPL do not apply).
//
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU
//  Affero General Public License as published by the Free Software Foundation, either version 3 of
//  the License, or (at your option) any later version. You should have received a copy of the GNU
//  Affero General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <memory>
#include <sstream>
#include "RightsService.h"
#include "Public/ILicense.h"
#include "Public/IRights.h"
#include "Public/IUser.h"
#include "Public/IStorageProvider.h"
#include "IRightsManager.h"

namespace lcp
{
    /* static */ int IRightsService::UNLIMITED = -1;
    
    RightsService::RightsService(IStorageProvider * storageProvider, const std::string & unknownUserId)
        : m_storageProvider(storageProvider)
        , m_unknownUserId(unknownUserId)
    {
    }

    void RightsService::SyncRightsFromStorage(ILicense * license)
    {
        IRightsManager * rightsManager = this->PerformChecks(license);
        std::string keyPrefix = this->BuildStorageProviderRightsKeyPrefix(license);

        std::unique_ptr<KvStringsIterator> rightsIt(m_storageProvider->EnumerateVault(LicenseRightsVaultId));
        for (rightsIt->First(); !rightsIt->IsDone(); rightsIt->Next())
        {
            std::string storageKey = rightsIt->CurrentKey();
            if (storageKey.find(keyPrefix) != std::string::npos)
            {
                std::string rightId = this->ExtractRightsKey(storageKey);
                rightsManager->SetRightValue(rightId, rightsIt->Current());
            }
        }
    }

    bool RightsService::CanUseRight(ILicense * license, const std::string & rightId) const
    {
        IRightsManager * rightsManager = this->PerformChecks(license);
        return rightsManager->CanUseRight(rightId);
    }

    bool RightsService::UseRight(ILicense * license, const std::string & rightId)
    {
        return this->UseRight(license, rightId, 1);
    }

    bool RightsService::UseRight(ILicense * license, const std::string & rightId, int amount)
    {
        IRightsManager * rightsManager = this->PerformChecks(license);
        if (rightsManager->UseRight(rightId, amount))
        {
            std::string currentValue;
            license->Rights()->GetRightValue(rightId, currentValue);

            m_storageProvider->SetValue(
                LicenseRightsVaultId,
                this->BuildStorageProviderRightsKey(license, rightId),
                currentValue
                );
            return true;
        }
        return false;
    }

    void RightsService::SetValue(ILicense * license, const std::string & rightId, const std::string & value)
    {
        IRightsManager * rightsManager = this->PerformChecks(license);
        rightsManager->SetRightValue(rightId, value);

        m_storageProvider->SetValue(
            LicenseRightsVaultId,
            this->BuildStorageProviderRightsKey(license, rightId),
            value
            );
    }

    std::string RightsService::GetValue(ILicense * license, const std::string & rightId) const
    {
        std::string value;
        if (license->Rights()->GetRightValue(rightId, value))
        {
            return value;
        }
        return std::string();
    }

    std::string RightsService::BuildStorageProviderRightsKey(ILicense * license, const std::string & rightId) const
    {
        std::string userId = license->User()->Id();
        if (userId.empty())
        {
            userId = m_unknownUserId;
        }
        std::stringstream keyStream;
        keyStream << license->Provider() << "@" << userId << "@" << license->Id() << "@" << rightId;
        return keyStream.str();
    }

    std::string RightsService::BuildStorageProviderRightsKeyPrefix(ILicense * license) const
    {
        std::string userId = license->User()->Id();
        if (userId.empty())
        {
            userId = m_unknownUserId;
        }
        std::stringstream keyStream;
        keyStream << license->Provider() << "@" << userId << "@" << license->Id();
        return keyStream.str();
    }

    std::string RightsService::ExtractRightsKey(const std::string & storageProviderKey) const
    {
        size_t pos = storageProviderKey.find_last_of("@");
        if (pos == std::string::npos || pos + 1 == storageProviderKey.size())
        {
            throw std::runtime_error("Wrong storage provider rights key format: " + storageProviderKey);
        }
        return storageProviderKey.substr(pos + 1);
    }

    IRightsManager * RightsService::PerformChecks(ILicense * license) const
    {
        if (m_storageProvider == nullptr)
        {
            throw std::runtime_error("StorageProvider is nullptr");
        }
        IRightsManager * rightsManager = dynamic_cast<IRightsManager *>(license->Rights());
        if (rightsManager == nullptr)
        {
            throw std::logic_error("Can not cast IRights to IRightsManager");
        }
        return rightsManager;
    }
}