// Copyright 2022 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// This file is part of eProsima Fast DDS Visualizer Plugin.
//
// eProsima Fast DDS Visualizer Plugin is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// eProsima Fast DDS Visualizer Plugin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with eProsima Fast DDS Visualizer Plugin. If not, see <https://www.gnu.org/licenses/>.

/**
 * @file dynamic_types_utils.cpp
 */

#include <sstream>
#include <random>
#include <limits>

#include <fastrtps/types/DynamicDataFactory.h>
#include <fastrtps/types/DynamicTypeMember.h>
#include <fastrtps/types/TypeDescriptor.h>

#include "dynamic_types_utils.hpp"
#include "utils.hpp"
#include "Exception.hpp"

namespace eprosima {
namespace plotjuggler {
namespace utils {

using namespace eprosima::fastrtps::types;

std::vector<std::string> get_introspection_type_names(
        const TypeIntrospectionCollection& numeric_type_names)
{
    std::vector<std::string> type_names;
    for (const auto& type_name : numeric_type_names)
    {
        type_names.push_back(std::get<types::DatumLabel>(type_name));
    }
    return type_names;
}

void get_introspection_type_names(
        const std::string& base_type_name,
        const DynamicType_ptr& type,
        const DataTypeConfiguration& data_type_configuration,
        TypeIntrospectionCollection& numeric_type_names,
        TypeIntrospectionCollection& string_type_names,
        DynamicData* data /* = nullptr */,
        const std::vector<MemberId>& current_members_tree /* = {} */,
        const std::vector<TypeKind>& current_kinds_tree /* = {} */,
        const std::string& separator /* = "/" */)
{
    // Get type kind and store it as kind tree
    TypeKind kind = type->get_kind();

    DEBUG("Getting types for type member " << base_type_name << " of kind " + std::to_string(kind));

    switch (kind)
    {
        case fastrtps::types::TK_BOOLEAN:
        case fastrtps::types::TK_BYTE:
        case fastrtps::types::TK_INT16:
        case fastrtps::types::TK_INT32:
        case fastrtps::types::TK_INT64:
        case fastrtps::types::TK_UINT16:
        case fastrtps::types::TK_UINT32:
        case fastrtps::types::TK_UINT64:
        case fastrtps::types::TK_FLOAT32:
        case fastrtps::types::TK_FLOAT64:
        case fastrtps::types::TK_FLOAT128:
            // Add numeric value
            numeric_type_names.push_back(
                {base_type_name, current_members_tree, current_kinds_tree, kind});
            break;

        case fastrtps::types::TK_CHAR8:
        case fastrtps::types::TK_CHAR16:
        case fastrtps::types::TK_STRING8:
        case fastrtps::types::TK_STRING16:
        case fastrtps::types::TK_ENUM:
            string_type_names.push_back(
                {base_type_name, current_members_tree, current_kinds_tree, kind});
            break;

        case fastrtps::types::TK_ARRAY:
        case fastrtps::types::TK_SEQUENCE:
        {
            DynamicType_ptr internal_type = type_internal_kind(type);

            std::string kind_str;
            unsigned int size;
            if (kind == fastrtps::types::TK_ARRAY)
            {
                kind_str = "array";
                size = array_size(type);
            }
            else if (kind == fastrtps::types::TK_SEQUENCE)
            {
                kind_str = "sequence";
                assert(data != nullptr);
                size = data->get_item_count();
            }

            // Allow this array/sequence depending on data type configuration
            if (size >= data_type_configuration.max_array_size)
            {
                if (data_type_configuration.discard_large_arrays)
                {
                    // Discard array/sequence
                    DEBUG("Discarding " << kind_str << " " << base_type_name << " of size " << size);
                    break;
                }
                else
                {
                    // Truncate array
                    DEBUG(
                        "Truncating " << kind_str << " " << base_type_name <<
                            " of size " << size <<
                            " to size " << data_type_configuration.max_array_size);
                    size = data_type_configuration.max_array_size;
                }
                // Could not be neither of them, it would be an inconsistency
            }

            for (MemberId member_id = 0; member_id < size; member_id++)
            {
                std::vector<MemberId> new_members_tree(current_members_tree);
                new_members_tree.push_back(member_id);
                std::vector<TypeKind> new_kinds_tree(current_kinds_tree);
                new_kinds_tree.push_back(kind);

                DynamicData* member_data = data ? data->loan_value(member_id) : nullptr;

                get_introspection_type_names(
                    base_type_name + "[" + std::to_string(member_id) + "]",
                    internal_type,
                    data_type_configuration,
                    numeric_type_names,
                    string_type_names,
                    member_data,
                    new_members_tree,
                    new_kinds_tree,
                    separator);

                if (member_data)
                {
                    data->return_loaned_value(member_data);
                }
            }
            break;
        }

        case fastrtps::types::TK_STRUCTURE:
        {
            // Using the Dynamic type, retrieve the name of the fields and its descriptions
            std::map<std::string, DynamicTypeMember*> members_by_name;
            type->get_all_members_by_name(members_by_name);

            DEBUG("Size of members " << members_by_name.size());

            for (const auto& members : members_by_name)
            {
                DEBUG("Calling introspection for " << members.first);

                std::vector<MemberId> new_members_tree(current_members_tree);
                new_members_tree.push_back(members.second->get_id());
                std::vector<TypeKind> new_kinds_tree(current_kinds_tree);
                new_kinds_tree.push_back(kind);

                DynamicData* member_data = data ? data->loan_value(members.second->get_id()) : nullptr;

                get_introspection_type_names(
                    base_type_name + separator + members.first,
                    members.second->get_descriptor()->get_type(),
                    data_type_configuration,
                    numeric_type_names,
                    string_type_names,
                    member_data,
                    new_members_tree,
                    new_kinds_tree,
                    separator);

                if (member_data)
                {
                    data->return_loaned_value(member_data);
                }
            }
            break;
        }

        case fastrtps::types::TK_BITSET:
        case fastrtps::types::TK_UNION:
        case fastrtps::types::TK_MAP:
        case fastrtps::types::TK_NONE:
        default:
            WARNING(kind << " DataKind Not supported");
            throw std::runtime_error("Unsupported Dynamic Types kind");
    }
}

void get_introspection_numeric_data(
        const TypeIntrospectionCollection& numeric_type_names,
        DynamicData* data,
        TypeIntrospectionNumericStruct& numeric_data_result)
{
    DEBUG("Getting numeric data");

    if (numeric_type_names.size() != numeric_data_result.size())
    {
        throw InconsistencyException("Vector of struct and result sizes mismatch in get_introspection_numeric_data");
    }

    // It is used by index of the vector to avoid unnecessary lookups.
    // Vectors must be sorted in the same order => creation order given in get_introspection_type_names
    for (int i = 0; i < numeric_type_names.size(); ++i)
    {
        const auto& member_type_info = numeric_type_names[i];

        // Get reference to variables to avoid calling get twice
        const auto& members =
                std::get<std::vector<MemberId>>(member_type_info);
        const auto& kinds =
                std::get<std::vector<TypeKind>>(member_type_info);

        const TypeKind& actual_kind =
                std::get<TypeKind>(member_type_info);

        // Get Data parent that has the member we are looking for
        auto parent_data = get_parent_data_of_member(
            data,
            members,
            kinds);

        numeric_data_result[i].second = get_numeric_type_from_data(
            parent_data,
            members[members.size() - 1], // use last member Id (direct parent = parent_data)
            actual_kind);    // use last kind (direct parent = parent_data)
    }
}

void get_introspection_string_data(
        const TypeIntrospectionCollection& string_type_names,
        DynamicData* data,
        TypeIntrospectionStringStruct& string_data_result)
{
    DEBUG("Getting string data");

    if (string_type_names.size() != string_data_result.size())
    {
        throw InconsistencyException("Vector of struct and result sizes mismatch in get_introspection_string_data");
    }

    // It is used by index of the vector to avoid unnecessary lookups.
    // Vectors must be sorted in the same order => creation order given in get_introspection_type_names
    for (int i = 0; i < string_type_names.size(); ++i)
    {
        const auto& member_type_info = string_type_names[i];

        // Get reference to variables to avoid calling get twice
        const auto& members =
                std::get<std::vector<MemberId>>(member_type_info);
        const auto& kinds =
                std::get<std::vector<TypeKind>>(member_type_info);

        const TypeKind& actual_kind =
                std::get<TypeKind>(member_type_info);

        // Get Data parent that has the member we are looking for
        auto parent_data = get_parent_data_of_member(
            data,
            members,
            kinds);

        string_data_result[i].second = get_string_type_from_data(
            parent_data,
            members[members.size() - 1], // use last member Id (direct parent = parent_data)
            actual_kind);    // use last kind (direct parent = parent_data)
    }
}

DynamicData* get_parent_data_of_member(
        DynamicData* data,
        const std::vector<MemberId>& members_tree,
        const std::vector<TypeKind>& kind_tree,
        unsigned int array_indexes /* = 0 */)
{

    DEBUG("Getting parent data of type " << std::to_string(kind_tree[array_indexes]));

    if (array_indexes == members_tree.size() - 1)
    {
        // One-to-last value, so this one is the value to take, as it is the parent of the data
        return data;
    }
    else
    {
        // Get the member and kind of the current level
        const MemberId& member_id = members_tree[array_indexes];
        const TypeKind& kind = kind_tree[array_indexes];

        switch (kind)
        {
            case fastrtps::types::TK_STRUCTURE:
            case fastrtps::types::TK_ARRAY:
            case fastrtps::types::TK_SEQUENCE:
            {
                // Access to the data inside the structure
                DynamicData* child_data;
                // Get data pointer to the child_data
                // The loan and return is a workaround to avoid creating an unnecessary copy of the data
                child_data = data->loan_value(member_id);
                data->return_loaned_value(child_data);

                return get_parent_data_of_member(
                    child_data,
                    members_tree,
                    kind_tree,
                    array_indexes + 1);
            }

            // TODO
            default:
                // TODO add exception
                WARNING("Error getting data");
                return data;
                break;
        }
    }
}

double get_numeric_type_from_data(
        DynamicData* data,
        const MemberId& member,
        const TypeKind& kind)
{
    DEBUG("Getting numeric data of kind " << std::to_string(kind) << " in member " << member);

    switch (kind)
    {
        case fastrtps::types::TK_BOOLEAN:
            return static_cast<double>(data->get_bool_value(member));

        case fastrtps::types::TK_BYTE:
            return static_cast<double>(data->get_byte_value(member));

        case fastrtps::types::TK_INT16:
            return static_cast<double>(data->get_int16_value(member));

        case fastrtps::types::TK_INT32:
            return static_cast<double>(data->get_int32_value(member));

        case fastrtps::types::TK_INT64:
            return static_cast<double>(data->get_int64_value(member));

        case fastrtps::types::TK_UINT16:
            return static_cast<double>(data->get_uint16_value(member));

        case fastrtps::types::TK_UINT32:
            return static_cast<double>(data->get_uint32_value(member));

        case fastrtps::types::TK_UINT64:
            return static_cast<double>(data->get_uint64_value(member));

        case fastrtps::types::TK_FLOAT32:
            return static_cast<double>(data->get_float32_value(member));

        case fastrtps::types::TK_FLOAT64:
            return static_cast<double>(data->get_float64_value(member));

        case fastrtps::types::TK_FLOAT128:
            return static_cast<double>(data->get_float128_value(member));

        default:
            // The rest of values should not arrive here (are cut before when creating type names)
            throw InconsistencyException("Member wrongly set as numeric");
    }
}

std::string get_string_type_from_data(
        DynamicData* data,
        const MemberId& member,
        const TypeKind& kind)
{
    DEBUG("Getting string data of kind " << std::to_string(kind) << " in member " << member);

    switch (kind)
    {
        case fastrtps::types::TK_CHAR8:
            return to_string(data->get_char8_value(member));

        case fastrtps::types::TK_CHAR16:
            return to_string(data->get_char16_value(member));

        case fastrtps::types::TK_STRING8:
            return data->get_string_value(member);

        case fastrtps::types::TK_STRING16:
            return to_string(data->get_wstring_value(member));

        case fastrtps::types::TK_ENUM:
            return data->get_enum_value(member);

        default:
            // The rest of values should not arrive here (are cut before when creating type names)
            throw InconsistencyException("Member wrongly set as string");
    }
}

bool is_kind_numeric(
        const TypeKind& kind)
{
    switch (kind)
    {
        case fastrtps::types::TK_BOOLEAN:
        case fastrtps::types::TK_BYTE:
        case fastrtps::types::TK_INT16:
        case fastrtps::types::TK_INT32:
        case fastrtps::types::TK_INT64:
        case fastrtps::types::TK_UINT16:
        case fastrtps::types::TK_UINT32:
        case fastrtps::types::TK_UINT64:
        case fastrtps::types::TK_FLOAT32:
        case fastrtps::types::TK_FLOAT64:
        case fastrtps::types::TK_FLOAT128:
            return true;

        default:
            return false;
    }
}

bool is_kind_string(
        const TypeKind& kind)
{
    switch (kind)
    {
        case fastrtps::types::TK_CHAR8:
        case fastrtps::types::TK_CHAR16:
        case fastrtps::types::TK_STRING8:
        case fastrtps::types::TK_STRING16:
        case fastrtps::types::TK_ENUM:
            return true;

        default:
            return false;
    }
}

DynamicType_ptr type_internal_kind(
        const DynamicType_ptr& dyn_type)
{
    return dyn_type->get_descriptor()->get_element_type();
}

unsigned int array_size(
        const DynamicType_ptr& dyn_type)
{
    return dyn_type->get_descriptor()->get_total_bounds();
}

bool is_type_static(
        const DynamicType_ptr& dyn_type)
{
    // Get type kind and store it as kind tree
    TypeKind kind = dyn_type->get_kind();

    switch (kind)
    {
        case fastrtps::types::TK_BOOLEAN:
        case fastrtps::types::TK_BYTE:
        case fastrtps::types::TK_INT16:
        case fastrtps::types::TK_INT32:
        case fastrtps::types::TK_INT64:
        case fastrtps::types::TK_UINT16:
        case fastrtps::types::TK_UINT32:
        case fastrtps::types::TK_UINT64:
        case fastrtps::types::TK_FLOAT32:
        case fastrtps::types::TK_FLOAT64:
        case fastrtps::types::TK_FLOAT128:
        case fastrtps::types::TK_CHAR8:
        case fastrtps::types::TK_CHAR16:
        case fastrtps::types::TK_STRING8:
        case fastrtps::types::TK_STRING16:
        case fastrtps::types::TK_ENUM:
        case fastrtps::types::TK_BITSET:
        case fastrtps::types::TK_BITMASK:
        case fastrtps::types::TK_UNION:
        case fastrtps::types::TK_ARRAY:
            return true;

        case fastrtps::types::TK_SEQUENCE:
        case fastrtps::types::TK_MAP:
            return false;

        case fastrtps::types::TK_STRUCTURE:
        {
            // Using the Dynamic type, retrieve the name of the fields and its descriptions
            std::map<std::string, DynamicTypeMember*> members_by_name;
            dyn_type->get_all_members_by_name(members_by_name);

            bool ret = true;
            for (const auto& members : members_by_name)
            {
                ret = ret & is_type_static(members.second->get_descriptor()->get_type());
            }
            return ret;
        }

        case fastrtps::types::TK_NONE:
        default:
            WARNING(kind << " DataKind Not supported");
            throw std::runtime_error("Unsupported Dynamic Types kind");
    }
}

} /* namespace utils */
} /* namespace plotjuggler */
} /* namespace eprosima */
