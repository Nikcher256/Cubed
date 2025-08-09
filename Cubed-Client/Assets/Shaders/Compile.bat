@echo off

if not exist bin (
    mkdir bin
)

call glslangValidator -V -o bin/basic.frag.spirv basic.frag.glsl
call glslangValidator -V -o bin/basic.vert.spirv basic.vert.glsl

pause
