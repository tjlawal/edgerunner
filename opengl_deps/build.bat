@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

miscl\ctime.exe -begin run_tree\edgerunner.ctm
REM --- @TODO: Add SIMD flags to toggle them. I just have them shoved in for now.

:: --- Unpack argument
for %%a in (%*) do set "%%a=1"
if not "%msvc%"=="1" if not "%clang%"=="1"	set msvc=1
if not "%release%"=="1"											set debug=1

if "%debug%"=="1" 													set release=0 && echo [debug mode]
if "%release%"=="1" 												set debug=0 	&& echo [release mode]
if "%clang%"=="1" 													set msvc=0    && echo [compiling with clang]
if "%msvc%"=="1" 			  										set clang=0		&& echo [compiling with msvc]

set auto_compile_flags=
if "%asan%"=="1" 														set auto_compile_flags=%auto_compile_flags% -fsanitize=address && echo [asan enabled]
if "%sp%"=="1" 															set auto_compile_flags=%auto_compile_flags% -DBUILD_PROFILE=1 -DPROFILER_SUPERLUMINAL=1 && echo [profiler enabled, using Superluminal profiler]
if "%tracy%"=="1" 	  											set auto_compile_flags=%auto_compile_flags% -DBUILD_PROFILE=1 -DPROFILER_TRACY=1 && echo [profiler enabled, using Tracy profiler]
if "%noisy%"=="1"														set auto_compile_flags=%auto_compile_flags% -DBUILD_DEBUG_VERY_NOISY=1 && echo [noisy build]
if "%avx512%"=="1"		(
	if "%clang%"=="1" 												set auto_compile_flags=-march=x86-64-v4
	if "%msvc%"=="1" 													set auto_compile_flags=/arch:AVX512
	echo [AVX-512 enabled]
)

:: clang writes the result of getting only the preprocessed code to whatever is pointed to by '-o'.
set preprocessor_flags=
if "%preprocess%"=="1" (
	if "%clang%"=="1" 												set preprocessor_flags=%preprocessor_flags% -E
	if "%msvc%"=="1"  												set preprocessor_flags=%preprocessor_flags% /P
	echo [preprocessor output only]
)

:: --- Clang 
set clang_common=   -I..\src\ -Wall -std=c++11 -march=x86-64-v3 -ferror-limit=15 -gcodeview -fdiagnostics-absolute-paths -fno-exceptions -Wno-initializer-overrides -Wno-unused-function -Wno-missing-braces -Wno-unused-variable -Wno-writable-strings -Wno-address-of-temporary -Wno-switch -Wno-return-type -Wno-unused-command-line-argument -Wno-unused-but-set-variable
set clang_debug=    call clang -g -O0 -DBUILD_DEBUG=1 %clang_common% %auto_compile_flags% %preprocessor_flags%
set clang_release=  call clang -g -O2 -DBUILD_DEBUG=0 -DBUILD_RELEASE=1 %clang_common% %auto_compile_flags%
set clang_link=     -fuse-ld=lld -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%%%%_PDB%%%% -Wl,/ignore:4099
set clang_out=      -o

:: --- MSVC
set cl_common=     /I..\src\ /nologo /FC /Z7 /EHsc /W1 /GR- /MT /arch:AVX2
set cl_debug=      call cl /Od /DBUILD_DEBUG=1 %cl_common% %auto_compile_flags% %preprocessor_flags%
set cl_release=    call cl /O2 /DBUILD_DEBUG=0 -DBUILD_RELEASE=1 %cl_common% %auto_compile_flags%
set cl_link=       /link /MANIFEST:EMBED /DEBUG:FULL /INCREMENTAL:NO /PDBALTPATH:%%%%_PDB%%%% /ignore:4099
set cl_out=        /out:

:: --- Shader Compiler - fxc
set fxc_common=		/nologo /Zpr /WX
set fxc_debug=		call fxc /Zi /Od %fxc_common%
set fxc_release=	call fxc /O1 %fxc_common%
set fxc_out=			/Fo

:: --- Windows resource file
set link_resource=resource.res
if "%msvc%"=="1" 						set rc=call rc
if "%clang%"=="1" 					set rc=call llvm-rc

:: --- Compile/Link
if "%msvc%"=="1"      			set compile_debug=%cl_debug%
if "%msvc%"=="1"      			set compile_release=%cl_release%
if "%msvc%"=="1"      			set compile_link=%cl_link%
if "%msvc%"=="1"      			set out=%cl_out%

if "%clang%"=="1"     			set compile_debug=%clang_debug%
if "%clang%"=="1"     			set compile_release=%clang_release%
if "%clang%"=="1"     			set compile_link=%clang_link%
if "%clang%"=="1"     			set out=%clang_out%

if "%debug%"=="1"     			set compile=%compile_debug%
if "%release%"=="1"   			set compile=%compile_release%

:: --- Prep directories
if not exist run_tree (
 mkdir run_tree
 echo "[WARNING] creating run_tree, data files do not exist."
)

:: Get subversion revision history
for /f "tokens=2" %%i in ('call svn info ^| findstr "Revision"') do set compile=%compile% -DBUILD_SVN_REVISION=%%i

:: Process rc files
pushd run_tree
	%rc% /nologo /fo resource.res .\data\resource.rc || exit /b 1
popd

:: --- Build Things
pushd run_tree
REM 	REM --- Delete previous build related files before compiling
	del /s *.pdb *.exe *.obj
	
	if "%hello%"=="1"					set didbuild=1 && %compile% ..\src\edgerunner\hello.cc %compile_link% %link_resource% %out%edgerunner.exe 		|| exit /b 1
popd

:: --- Warn On No Builds ------------------------------------------------------
if "%didbuild%"=="" (
  echo [WARNING] no valid build target specified; must use build target names as arguments to this script, like `build edge_runner` or `build tests`.
  exit /b 1
)

miscl\ctime.exe -end run_tree\edgerunner.ctm
endlocal
