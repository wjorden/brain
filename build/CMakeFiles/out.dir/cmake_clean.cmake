file(REMOVE_RECURSE
  "libout.a"
  "libout.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/out.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
