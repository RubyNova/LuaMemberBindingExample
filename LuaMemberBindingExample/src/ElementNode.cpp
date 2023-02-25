// Copyright © Matt Jones and Contributors. Licensed under the MIT Licence (MIT). See LICENCE.md in the repository root
// for more information.

#include <ElementNode.hpp>
#include <iostream>

void ElementNode::SayHelloWorld() const noexcept
{
    std::cout << "Hello from C++!" << std::endl;
}

int32_t ElementNode::Add(int32_t lhs, int32_t rhs) const noexcept
{
    return lhs + rhs;
}

void ElementNode::SetPointlessBool(bool value) noexcept
{
    pointlessBool = value;
}
