// Copyright © Matt Jones and Contributors. Licensed under the MIT Licence (MIT). See LICENCE.md in the repository root
// for more information.


#ifndef LUATABLEDATAENTRY_HPP
#define LUATABLEDATAENTRY_HPP

#include <any>
#include <string>

enum class DataType
{
    Unknown,
    String,
    Double,
    Integer
};

struct LuaTableDataEntry
{
    std::any data;
    std::string memberName;
    DataType type;
};

#endif