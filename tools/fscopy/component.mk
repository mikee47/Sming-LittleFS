COMPONENT_DEPENDS := LittleFS
HWCONFIG := standard

# Call the generated executable something different (by default, it's just `app`)
APP_NAME := fscopy

# We don't need heap monitoring for utility applications
ENABLE_MALLOC_COUNT := 0

#
HOST_NETWORK_OPTIONS := --nonet --debug=0
