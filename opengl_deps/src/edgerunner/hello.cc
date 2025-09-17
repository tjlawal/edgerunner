#include "basic/basic.h"
#include "platform/platform.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

void process_input(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}

glenum gl_check_error_(const char *file, s32 line) {
	glenum error_code = 0;
	while((error_code == glGetError	()) != GL_NO_ERROR) {
		std::string error;
		switch(error_code) {
			case GL_INVALID_ENUM: 			error = "INVALID_ENUM"; break;
			case GL_INVALID_VALUE: 			error = "INVALID_VALUE"; break;
			case GL_INVALID_OPERATION: 	error = "INVALID_OPERATION"; break;
			case GL_STACK_OVERFLOW: 		error = "STACK_OVERFLOW"; break;
			case GL_STACK_UNDERFLOW: 		error = "STACK_UNDERFLOW"; break;
			case GL_OUT_OF_MEMORY: 			error = "OUT_OF_MEMORY"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
		}

		std::cout << error << " | " << file << " (" << line << ") \n"; 
	}
	return error_code;
}

#define gl_check_error() gl_check_error_(__FILE__, __LINE__)

// FPS counter
static f64 previous;
static f64 frame_count;
void update_fps_counter(GLFWwindow* window) {
	f64 current = glfwGetTime();
	f64 elapsed = current - previous;
	
	// fps is updated every 4 frames per second!
	if(elapsed > 0.25) {
		previous = current;
		char tmp[128];
		f64 fps = (f64)frame_count / elapsed;
		sprintf(tmp, "Edgerunner - FPS: %.2f", fps);
		glfwSetWindowTitle(window, tmp);
		frame_count = 0;
	}
	frame_count++;
}

int main() {
	glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(1280, 720, "Edgerunner", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // No need for vsync yet, current rendering done is pretty benign. 
	// Downside is it uses the entire resources on the GPU.

	// Tell GLFW to resize the framebuffer on window resize.
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD\n";
    return -1;
  }

	// Get gpu information
	const glubyte* vendor = glGetString(GL_VENDOR);
	const glubyte* renderer = glGetString(GL_RENDERER);
	const glubyte* version = glGetString(GL_VERSION);
	const glubyte* shading_lang_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
	int max_vertex_attr;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attr);

	std::cout << "Vendor: " << vendor << "\n";
	std::cout << "Renderer: " << renderer << "\n";
	std::cout << "OpenGL Version: " << version << "\n";
	std::cout << "GLSL Version: " << shading_lang_version << "\n";

	// Triangle
	f32 g_vertices[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		0.0f,  0.5f, 0.0f
	};

	
	// Vertex shader
	const char *vertex_shader_source = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

	u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);
	
	{
		int  success;
		char infoLog[512];
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
		}
	}

	// Fragment shader
	const char *fragment_shader_source = "#version 330 core\n"
	" out vec4 FragColor;\n"
	"void main() {\n"
  "  FragColor = vec4(0.27183728f, 0.284972084f, 0.01998319f, 1.0f);\n"
	"}\0";

	u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1 , &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);
	
	{
		int  success;
		char infoLog[512];
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
		}
	}

	u32 shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);
	{
		int success;
		char infoLog[512];
		glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
		if(!success) {
			glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::LINKING_FAILED\n" << infoLog << std::endl;
		}
	}

	//glDeleteShader(vertex_shader);
	//glDeleteShader(fragment_shader);

	u32 vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertices), g_vertices, GL_STATIC_DRAW);
	
	u32 vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void*)0);


	while(!glfwWindowShouldClose(window)) {
    process_input(window);

		update_fps_counter(window);

    // rendering commands here
    glClearColor(0.2f, 0.3f, 1.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shader_program);
		glBindVertexArray(vao);

		glDrawArrays(GL_TRIANGLES, 0, 3);

    glfwSwapBuffers(window);
    glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}