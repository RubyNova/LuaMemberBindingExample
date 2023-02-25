#include <iostream>
#include <LuaManager.hpp>
#include <LuaTypeRegistry.hpp>
#include <ElementNode.hpp>

int main(int, char**) {

    LuaManager manager{};
    LuaTypeRegistry<ElementNode> registry("ElementNode");

    registry.RegisterMethod("SayHello", [](auto L) {
        ElementNode* node = static_cast<ElementNode*>(
            luaL_checkudata(L, 1, "ElementNode"));
        node->SayHelloWorld();
        return 0;
    });
    registry.RegisterMethod("SetPointlessBool", [](auto L) {
        ElementNode* node = static_cast<ElementNode*>(
            luaL_checkudata(L, 1, "ElementNode"));

        node->SetPointlessBool(lua_toboolean(L, 2));
        return 0;
    });
    registry.RegisterMethod("Add", [](auto L) {
        ElementNode* node = static_cast<ElementNode*>(
            luaL_checkudata(L, 1, "ElementNode"));

        lua_pushinteger(L,
            node->Add(
                static_cast<int32_t>(luaL_checkinteger(L, 2)),
                static_cast<int32_t>(luaL_checkinteger(L, 3))));
        return 1;
    });
    registry.RegisterFreeFunction("SaySomething", [](auto) {
        std::cout << "Hello from C++ (really cool edition)!!!\n";
        return 0;
    });
    registry.RegisterField("PointlessBool", &ElementNode::pointlessBool);

    manager.ApplyRegistry(registry);

    auto node = manager.Instantiate(registry); // you can do stuff with the object here if you want. We're just casting to void to shut the compiler up.
    static_cast<void>(node);
    manager.SetGlobal("node"); // globals aren't the best for everything, upvalues make more sense in local variable situations, but globals are easiest for proof of concept.

    manager.Execute("ElementNode.SaySomething()");
    manager.Execute("local myNode = ElementNode.Create() myNode:SayHello()");
    //manager.Execute("node:SayHello()");
    manager.Execute("print(node.PointlessBool)");

    for (int i = 0; i < 5; i++)
    {
        manager.Execute("node:SetPointlessBool(not node.PointlessBool) print(node.PointlessBool)");
    }

    manager.Execute("for i = 1, 10 do print(node:Add(i, 5)) end");
}
