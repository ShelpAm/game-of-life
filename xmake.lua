set_languages("c++latest")
add_includedirs("third-party/fast_io/include")

target("main")
    set_kind("binary")
    add_files("src/main.cpp")
