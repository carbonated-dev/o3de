/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/SpecializationConstant.h>

namespace AZ::RHI
{
    bool SpecializationConstant::operator==(const SpecializationConstant& rhs) const
    {
        return
            m_value == rhs.m_value &&
            m_name == rhs.m_name &&
            m_id == rhs.m_id &&
            m_type == rhs.m_type;
    }

    HashValue64 SpecializationConstant::GetHash() const
    {
        AZ::HashValue64 seed = AZ::HashValue64{ 0 };
        seed = TypeHash64(m_value.GetIndex(), seed);
        seed = TypeHash64(m_name.GetHash(), seed);
        seed = TypeHash64(m_id, seed);
        seed = TypeHash64(m_type, seed);
        return seed;
    }

    SpecializationData::SpecializationData(
        Key key,
        HashValue64 domainHash,
        HashValue64 constantsHash,
        MaterializeFunction materializeFunction)
        : m_key(AZStd::move(key))
        , m_domainHash(domainHash)
        , m_constantsHash(constantsHash)
        , m_materializeFunction(AZStd::move(materializeFunction))
    {
    }

    const SpecializationData::Key& SpecializationData::GetKey() const
    {
        return m_key;
    }

    HashValue64 SpecializationData::GetDomainHash() const
    {
        return m_domainHash;
    }

    HashValue64 SpecializationData::GetConstantsHash() const
    {
        return m_constantsHash;
    }

    bool SpecializationData::IsEquivalent(const SpecializationData& rhs) const
    {
        return m_domainHash == rhs.m_domainHash &&
            m_constantsHash == rhs.m_constantsHash &&
            m_key == rhs.m_key;
    }

    const AZStd::vector<SpecializationConstant>& SpecializationData::GetConstants() const
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_materializeMutex);
        if (m_materializeFunction)
        {
            m_materializeFunction(m_constants);
            m_materializeFunction = {};
        }
        return m_constants;
    }
}
