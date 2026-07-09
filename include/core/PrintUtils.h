#pragma once

#include <iostream>
#include <string>
#include <vector>

inline void printStatsBlock(const std::string& title, const std::vector<std::string>& lines)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;

    for (const std::string& line : lines)
        std::cout << "  " << line << std::endl;

    std::cout << "========================================\n" << std::endl;
}