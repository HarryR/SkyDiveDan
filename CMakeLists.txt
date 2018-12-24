cmake_minimum_required(VERSION 3.8)
project(skydivedan)

find_library(SDL SDL)
find_library(SDL_gfx SDL_gfx)

add_executable(skydivedan skydiver.cc)
target_link_libraries(skydivedan SDL SDL_gfx)
