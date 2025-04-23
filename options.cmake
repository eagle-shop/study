function(set_common_compile_options target)
  target_compile_options(${target}
    PUBLIC
      -Wall
      -Wextra
      -Werror
      -Wpedantic
      -Wconversion
      -Wshadow
      -Wnull-dereference
      -Wformat=2
      -fstack-protector-strong
      -D_FORTIFY_SOURCE=2
      -fPIC
      -fstack-clash-protection
  )

  target_link_options(${target}
    PUBLIC
      -Wl,-z,relro
      -Wl,-z,now
      -Wl,-z,noexecstack
  )
endfunction()
