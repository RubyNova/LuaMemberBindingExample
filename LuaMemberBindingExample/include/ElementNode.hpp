// Copyright © Matt Jones and Contributors. Licensed under the MIT Licence (MIT). See LICENCE.md in the repository root
// for more information.


#ifndef ELEMENTNODE_H
#define ELEMENTNODE_H

#include <cstdint>
#include <string>

class ElementNode
{
public:
    bool pointlessBool;
    std::string pointlessString;
    ElementNode() = default;

    void SayHelloWorld() const noexcept;
    [[nodiscard]] int32_t Add(int32_t lhs, int32_t rhs) const noexcept;
    void SetPointlessBool(bool value) noexcept; // this literally exists for testing purposes
};

#endif