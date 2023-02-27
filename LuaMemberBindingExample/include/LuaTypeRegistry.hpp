// Copyright © Matt Jones and Contributors. Licensed under the MIT Licence (MIT). See LICENCE.md in the repository root
// for more information.


#ifndef LUAFUNCTIONREGISTRY_HPP
#define LUAFUNCTIONREGISTRY_HPP

#include<algorithm>
#include <functional>
#include <lua.hpp>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <string>

struct FieldReadWriter
{
    using FieldAccessorType = std::function<void(void*,lua_State*)>;

    FieldAccessorType getter;
    FieldAccessorType setter;
};

class LuaTypeRegistryBase
{
public:
    using FunctionType = std::function<int(lua_State*)>;
    using Member = std::variant<FunctionType, FieldReadWriter>;
    using OptionalMemberRef = std::optional<std::reference_wrapper<const Member>>;

protected:
    std::string _typeName;
    std::vector<std::reference_wrapper<const LuaTypeRegistryBase>> _baseTypeRegistries;
    std::map<std::string, Member> _wrappedMembers;
    std::map<std::string, FunctionType> _freeFunctions;

    LuaTypeRegistryBase(std::string typeName, std::span<std::reference_wrapper<const LuaTypeRegistryBase>> baseTypeRegistries) noexcept
        : _typeName(typeName),
            _baseTypeRegistries(baseTypeRegistries.begin(), baseTypeRegistries.end()),
            _wrappedMembers(),
            _freeFunctions()
        {}
public:
    [[nodiscard]] inline const std::string& GetTypeName() const noexcept
    {
        return _typeName;
    }
    
    [[nodiscard]] inline bool HasBaseRegistries() const noexcept
    {
        return !_baseTypeRegistries.empty();
    }

    [[nodiscard]] inline const std::vector<std::reference_wrapper<const LuaTypeRegistryBase>>& GetBaseRegistries() const noexcept
    {
        return _baseTypeRegistries; // TODO: This should really be wrapped in std::span since its C++20 but its not cooperating lol
    }

    [[nodiscard]] OptionalMemberRef FindNamedMember(std::string_view member) const noexcept
    {
        auto it = std::find_if(
            _wrappedMembers.cbegin(),
            _wrappedMembers.cend(),
            [member](auto pair){
                return member.compare(pair.first) == 0;
            });

        if (it != _wrappedMembers.cend())
        {
            return it->second;
        }

        for (const auto& registry : _baseTypeRegistries)
        {
            auto found = registry.get().FindNamedMember(member);
            if (found)
            {
                return found;
            }
        }

        return std::nullopt;
    }
};

template<typename> inline constexpr bool always_false_v = false;

template <typename T>
class LuaTypeRegistry : public LuaTypeRegistryBase
{
public:
    using MemberType = std::variant<bool T::*, const char* T::*, int32_t T::*, std::string T::*>;

private:
    static int LookupMember(lua_State* L)
    {
        // lua_upvalueindex converts an upvalue index to a magic stack index
        LuaTypeRegistry* self = static_cast<LuaTypeRegistry*>(
            lua_touserdata(L, lua_upvalueindex(1)));

        void* value = luaL_checkudata(L, 1, self->GetTypeName().c_str());
        size_t length;
        const char* memberName = luaL_checklstring(L, 2, &length);

        auto member = self->FindNamedMember(std::string_view{memberName, length});
        if (!member)
        {
            return luaL_error(L, "failed to find key '%s'", memberName);
        }

        return std::visit([value, L](auto&& member) {
            using TMember = std::decay_t<decltype(member)>;
            if constexpr (std::is_same_v<TMember, FunctionType>)
            {
                lua_pushlightuserdata(L, const_cast<void*>(static_cast<const void*>(&member)));
                auto f = [](auto L){
                    TMember* member = static_cast<TMember*>(lua_touserdata(L, lua_upvalueindex(1)));
                    return member->operator()(L);
                };
                lua_pushcclosure(L, f, 1);
                return 1;
            }
            else if constexpr(std::is_same_v<TMember, FieldReadWriter>)
            {
                member.getter(value, L);
                return 1;
            }
            else
            {
                static_assert(always_false_v<TMember>, "non-exhaustive visitor");
                // unreachable but might be needed to make compiler happy
                return lua_error(L);
            }
        }, member.value().get());
    }

    static int AssignMember(lua_State* L)
    {
        // lua_upvalueindex converts an upvalue index to a magic stack index
        LuaTypeRegistry* self = static_cast<LuaTypeRegistry*>(
            lua_touserdata(L, lua_upvalueindex(1)));

        void* value = luaL_checkudata(L, 1, self->GetTypeName().c_str());
        
        size_t length;
        const char* memberName = luaL_checklstring(L, 2, &length);

        auto member = self->FindNamedMember(std::string_view{memberName, length});
        if (!member)
        {
            return luaL_error(L, "failed to find key '%s'", memberName);
        }

        return std::visit([value, L, memberName](auto&& member) {
            using TMember = std::decay_t<decltype(member)>;
            if constexpr(std::is_same_v<TMember, FieldReadWriter>)
            {
                member.setter(value, L);
                return 0;
            }
            else
            {
                return luaL_error(L, "Expected field with name '%s', got member function.", memberName); //TODO: this error probably isn't very good
            }
        }, member.value().get());
    }

    static int CleanupObject(lua_State* L)
    {
        // lua_upvalueindex converts an upvalue index to a magic stack index
        LuaTypeRegistry* self = static_cast<LuaTypeRegistry*>(
            lua_touserdata(L, lua_upvalueindex(1)));

        T* value = static_cast<T*>(luaL_checkudata(L, 1, self->GetTypeName().c_str()));
        value->~T();
        return 0;
    }

    static int CreateObject(lua_State* L)
    {
        LuaTypeRegistry* self = static_cast<LuaTypeRegistry*>(
            lua_touserdata(L, lua_upvalueindex(1)));

        static_cast<void>(self->Allocate(L));

        return 1;
    }

public:
    LuaTypeRegistry(std::string typeName, std::span<std::reference_wrapper<const LuaTypeRegistryBase>> baseTypeRegistries) noexcept
        : LuaTypeRegistryBase(typeName, baseTypeRegistries)
        {}

    explicit LuaTypeRegistry(std::string typeName) noexcept
        : LuaTypeRegistry(
            typeName,
            std::span<std::reference_wrapper<const LuaTypeRegistryBase>>{})
        {}

    void RegisterMethod(const std::string& name, FunctionType func)
    {
        if (_wrappedMembers.find(name) != _wrappedMembers.end())
        {
            throw std::runtime_error("A Lua type registry cannot have duplicate members.");
        }

        _wrappedMembers.emplace(name, func);
    }

    void RegisterFreeFunction(const std::string& name, FunctionType func)
    {
        if (_freeFunctions.find(name) != _freeFunctions.end())
        {
            throw std::runtime_error("A Lua type registry cannot have duplicate free functions.");
        }

        _freeFunctions.emplace(name, func);
    }

    template <typename TMember>
    void RegisterField(const std::string& name, TMember T::* member)
    {
        if (_wrappedMembers.find(name) != _wrappedMembers.end())
        {
            throw std::runtime_error("A Lua type registry cannot have duplicate members.");
        }

        _wrappedMembers.emplace(name,
        FieldReadWriter 
        {
            [member](auto wrappedValue, auto L)
            {
                T* value = static_cast<T*>(wrappedValue);
                if constexpr (std::is_same_v<TMember, bool>)
                {
                    lua_pushboolean(L, value->*member ? 1 : 0);
                }
                else if constexpr (std::is_same_v<TMember, const char*>)
                {
                    lua_pushstring(L, value->*member);
                }
                else if constexpr (std::is_same_v<TMember, std::string>)
                {
                    lua_pushstring(L, value->*member.c_str());
                }
                else if constexpr (std::is_same_v<TMember, int32_t>)
                {
                    lua_pushnumber(L, static_cast<lua_Number>(value->*member));
                }
                else
                {
                    static_assert(always_false_v<TMember>, "non-exhaustive visitor");
                }
            },
            [member](auto wrappedValue, auto L)
            {
                auto logTypeError = [](lua_State* L, const char* expected)
                {
                    int luaType = lua_type(L, -1);
                    luaL_error(L, "Expected boolean, got %s.", expected, lua_typename(L, luaType)); // TODO: Figure out the return stuff
                };

                T* value = static_cast<T*>(wrappedValue);
                if constexpr (std::is_same_v<TMember, bool>)
                {
                    if (!lua_isboolean(L, -1))
                    {
                        logTypeError(L, "boolean"); // TODO: Fix this and return somehow.
                        return;
                    }

                    bool result = static_cast<bool>(lua_toboolean(L, -1));
                    value->*member = result;
                    lua_pop(L, -1);
                }
                else if constexpr (std::is_same_v<TMember, const char*>)
                {
                    if (!lua_isstring(L, -1))
                    {
                        logTypeError(L, "string"); // TODO: Fix this and return somehow.
                        return;
                    }

                    const char* result = lua_tostring(L, -1);
                    value->*member = new const char*[strlen(result) + 1];
                    strcpy_s(value->*member, result);
                    lua_pop(L, -1);
                }
                else if constexpr (std::is_same_v<TMember, std::string>)
                {
                    if (!lua_isstring(L, -1))
                    {
                        logTypeError(L, "string"); // TODO: Fix this and return somehow.
                        return;
                    }

                    const char* result = lua_tostring(L, -1);
                    value->*member = std::string(result);
                    lua_pop(L, -1);
                }
                else if constexpr (std::is_same_v<TMember, int32_t>)
                {
                    if (!lua_isnumber(L, -1))
                    {
                        logTypeError(L, "number"); // TODO: Fix this and return somehow.
                        return;
                    }

                    LUA_NUMBER number = lua_tonumber(L, -1);
                    value->*member = static_cast<int32_t>(number);
                    lua_pop(L, -1);
                }
                else
                {
                    static_assert(always_false_v<TMember>, "non-exhaustive visitor");
                }
            }
        });
    }

    void GenerateBindings(lua_State* L) const
    {
        if (!luaL_newmetatable(L, GetTypeName().c_str()))
        {
            throw std::runtime_error("This Lua type already exists");
        }

        luaL_Reg metamethods[] = {
            {"__index", LookupMember},
            {"__gc", CleanupObject},
            {"__newindex", AssignMember},
            {nullptr, nullptr}
        };

        // push our upvalue first since luaL_setfuncs wants the table at the top
        // of the stack
        lua_pushlightuserdata(L, const_cast<void*>(static_cast<const void*>(this)));

        // use one updata value to keep a reference to this
        luaL_setfuncs(L, metamethods, 1);
        lua_pop(L, 1);

        lua_createtable(L, 0, static_cast<int>(_freeFunctions.size() + 1));
        for (const auto& pair : _freeFunctions)
        {
            lua_pushstring(L, pair.first.c_str());
            lua_pushlightuserdata(L, const_cast<void*>(static_cast<const void*>(&pair.second)));
            lua_pushcclosure(L, [](lua_State* L) {
                FunctionType* function = static_cast<FunctionType*>(lua_touserdata(L, lua_upvalueindex(1)));
                return function->operator()(L);
            }, 1);
            lua_rawset(L, -3);
        }

        lua_pushliteral(L, "Create");
        lua_pushlightuserdata(L, const_cast<void*>(static_cast<const void*>(this)));
        lua_pushcclosure(L, CreateObject, 1);
        lua_rawset(L, -3);

        lua_setglobal(L, GetTypeName().c_str());
    }

    template <typename... Args>
    T* Allocate(lua_State* L, Args&&... args)
    {
        void* ptr = lua_newuserdata(L, sizeof(T));
        luaL_setmetatable(L, _typeName.c_str());

        T* value = new (ptr) T(std::forward(args)...);
        return value;
    }
};

#endif