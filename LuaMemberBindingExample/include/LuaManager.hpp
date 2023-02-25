// Copyright © Matt Jones and Contributors. Licensed under the MIT Licence (MIT). See LICENCE.md in the repository root
// for more information.


#ifndef LUAMANAGER_H
#define LUAMANAGER_H

#include <lua.hpp>
#include <typeinfo>
#include <map>
#include <LuaTypeRegistry.hpp>

class LuaManager
{
private:
    lua_State* L;

public:
    LuaManager();
    ~LuaManager();

    template<typename T>
    void ApplyRegistry(const LuaTypeRegistry<T>& typeRegistry)
    {
        typeRegistry.GenerateBindings(L);
    }

    template<typename T>
    T* Instantiate(LuaTypeRegistry<T>& typeRegistry)
    {
        return typeRegistry.Allocate(L);
    }

    void Execute(std::string code);
    void SetGlobal(std::string name);
};

#endif