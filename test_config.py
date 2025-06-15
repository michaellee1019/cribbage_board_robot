Import("env")

# Add include paths for test stubs
env.Append(CPPPATH=["test/stubs"])

# Define test-specific flags
env.Append(CPPDEFINES=[
    ("UNIT_TEST", 1),
    ("ARDUINO", 100),
    ("ESP32", 1)
])

# Ensure we're using C++17
env.Append(CXXFLAGS=["-std=c++17"])

# Exclude main.cpp from the build to avoid Arduino setup/loop requirements
env.Replace(SRC_FILTER=[
    "+<*>",
    "-<.git/>",
    "-<.svn/>",
    "-<src/main.cpp>"  # Exclude main.cpp to avoid Arduino setup/loop requirements
])

# Print some debug info
print("Test environment configured with stubs")
print("Excluded src/main.cpp from build")
