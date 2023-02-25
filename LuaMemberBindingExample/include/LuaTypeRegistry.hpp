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

class LuaTypeRegistryBase
{
public:
    using FunctionType = std::function<int(lua_State*)>;
    using MemberAccessorType = std::function<void(void*,lua_State*)>;
    using Member = std::variant<FunctionType, MemberAccessorType>;
    using OptionalMemberRef = std::optional<std::reference_wrapper<const Member>>;

protected:
    std::string _typeName;
    std::vector<std::reference_wrapper<const LuaTypeRegistryBase>> _baseTypeRegistries;
    std::map<std::string, Member> _wrappedMembers;

    LuaTypeRegistryBase(std::string typeName, std::span<std::reference_wrapper<const LuaTypeRegistryBase>> baseTypeRegistries) noexcept
        : _typeName(typeName),
            _baseTypeRegistries(baseTypeRegistries.begin(), baseTypeRegistries.end()),
            _wrappedMembers()
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
    using MemberType = std::variant<bool T::*>;

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
            else if constexpr(std::is_same_v<TMember, MemberAccessorType>)
            {
                member(value, L);
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

    static int CleanupObject(lua_State* L)
    {
        // lua_upvalueindex converts an upvalue index to a magic stack index
        LuaTypeRegistry* self = static_cast<LuaTypeRegistry*>(
            lua_touserdata(L, lua_upvalueindex(1)));

        T* value = static_cast<T*>(luaL_checkudata(L, 1, self->GetTypeName().c_str()));
        value->~T();
        return 0;
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

    void RegisterFunction(const std::string& name, FunctionType func)
    {
        if (_wrappedMembers.find(name) != _wrappedMembers.end())
        {
            throw std::runtime_error("A Lua type registry cannot have duplicate members.");
        }

        _wrappedMembers.emplace(name, func);
    }

    template <typename TMember>
    void RegisterField(const std::string& name, TMember T::* member)
    {
        if (_wrappedMembers.find(name) != _wrappedMembers.end())
        {
            throw std::runtime_error("A Lua type registry cannot have duplicate members.");
        }

        _wrappedMembers.emplace(name, [member](auto wrappedValue, auto L)
        {
            T* value = static_cast<T*>(wrappedValue);
            if constexpr (std::is_same_v<TMember, bool>)
            {
                lua_pushboolean(L, value->*member ? 1 : 0);
            }
            else
            {
                static_assert(always_false_v<TMember>, "non-exhaustive visitor");
            }
        });
    }

    void GenerateBindings(lua_State* L) const
    {
        if (!luaL_newmetatable(L, _typeName.c_str()))
        {
            throw std::runtime_error("This Lua type already exists");
        }

        luaL_Reg metamethods[] = {
            {"__index", LookupMember},
            {"__gc", CleanupObject},
            {nullptr, nullptr}
        };

        // push our upvalue first since luaL_setfuncs wants the table at the top
        // of the stack
        lua_pushlightuserdata(L, const_cast<void*>(static_cast<const void*>(this)));

        // use one updata value to keep a reference to this
        luaL_setfuncs(L, metamethods, 1);
        lua_pop(L, 1);
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