add_custom_target(MiscThirdParty SOURCES dtoa.c xxhash/LICENSE xxhash/xxhash.c xxhash/xxhash.h)
set_target_properties(MiscThirdParty PROPERTIES FOLDER "Third party")