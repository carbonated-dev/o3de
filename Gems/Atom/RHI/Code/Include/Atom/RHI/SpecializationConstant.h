/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Utils/TypeHash.h>
#include <Atom/RHI.Reflect/Handle.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace AZ::RHI
{
    //! Holds a value for a specialization constant
    using SpecializationValue = RHI::Handle<uint32_t, struct SpecializationConstant>;

    //! Supported types for specialization constants
    enum class SpecializationType : uint32_t
    {
        Integer,
        Bool,
        Invalid
    };

    //! Contains all the necessary information and value of a specialization constant
    //! so it can be used when creating a PipelineState.
    struct SpecializationConstant
    {
        SpecializationConstant() = default;

        //! Name of the constant
        Name m_name;
        //! Id of the constant
        uint32_t m_id = 0;
        //! Value of the constant
        SpecializationValue m_value;
        //! Type of the constant
        SpecializationType m_type = SpecializationType::Invalid;

        bool operator==(const SpecializationConstant& rhs) const;
        //! Returns a hash of the constant
        HashValue64 GetHash() const;
    };

    //! Immutable identity and lazily materialized values for a set of specialization constants.
    //! Pipeline-state cache lookup uses the compact key and precomputed hash. The full constant
    //! vector is only constructed if an RHI backend needs to compile a cache miss.
    class SpecializationData final
    {
    public:
        using Key = AZStd::fixed_vector<uint32_t, 4>;
        using MaterializeFunction = AZStd::function<void(AZStd::vector<SpecializationConstant>&)>;

        SpecializationData(
            Key key,
            HashValue64 domainHash,
            HashValue64 constantsHash,
            MaterializeFunction materializeFunction);

        const Key& GetKey() const;
        HashValue64 GetDomainHash() const;
        HashValue64 GetConstantsHash() const;
        bool IsEquivalent(const SpecializationData& rhs) const;

        //! Materializes the full backend-facing representation on first use.
        const AZStd::vector<SpecializationConstant>& GetConstants() const;

    private:
        Key m_key;
        HashValue64 m_domainHash = HashValue64{ 0 };
        HashValue64 m_constantsHash = HashValue64{ 0 };
        mutable AZStd::mutex m_materializeMutex;
        mutable MaterializeFunction m_materializeFunction;
        mutable AZStd::vector<SpecializationConstant> m_constants;
    };

    using SpecializationDataPtr = AZStd::shared_ptr<const SpecializationData>;
}
