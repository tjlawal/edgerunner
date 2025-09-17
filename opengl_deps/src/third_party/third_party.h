#ifndef THIRD_PARTY_INC_H
#define THIRD_PARTY_INC_H

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#include "stb/stb_image.h"

#include "glad/glad.h"
#include "glad/glad.c"

#include "glfw/glfw3.h"
#pragma comment(lib, "../src/third_party/glfw/glfw3_mt")


#if BUILD_PROFILE 
	#if PROFILER_SUPERLUMINAL && OS_WINDOWS
		#include "Superluminal/include/PerformanceAPI.h"
		#pragma comment(lib, "../src/third_party/Superluminal/libs/PerformanceAPI_MT")
	#elif PROFILER_TRACY && OS_WINDOWS
		#define TRACY_ENABLE
		#include "tracy/include/tracy/Tracy.hpp"
		#include "tracy/include/TracyClient.cpp"
		#pragma comment(lib, "../src/third_party/tracy/libs/TracyClient")
	#else 
		#error "Profiler not recognized, are you using the correct profiler flag?"
	#endif
#endif

#endif // THIRD_PARTY_INC_H
