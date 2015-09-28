/*
 * install git clone https://github.com/PDKK/freetypeGlesRpi
 * How to compile
 * gcc image.c -L/opt/vc/lib -lbcm_host -lOpenVG -lEGL -lGLESv2 -lGL -lpng -lpthread -I /opt/vc/include 
 *   -I /opt/vc/include/interface/vcos/pthreads -I /opt/vc/include/interface/vmcs_host/linux -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

#include "texture-atlas.h"
#include "texture-font.h"
#include "support.h"
#include "input.h"
#include "keys.h"
#include <stdbool.h>
#include <string.h>
#include <wchar.h>

#define TRUE 				1
#define FALSE				0
#define BCM2708_PERI_BASE	0x20000000
#define GPIO_BASE			(BCM2708_PERI_BASE + 0x200000)
#define INTERVAL			30		//in milliseconds software button press detection
#define radious_of_circle	0.45
//mesh
#define NUM_VERTICES		4
#define NUM_VERTICES_TEXT	4*7 	//asuming max image files 999/999 -7 char
#define NUM_MAX_TEXT		7		//asuming max 7 char
#define NUM_INDICES			6		//2 triangles
//shader
#define NUM_SHADERS 		2
#define NUM_UNIFORMS		3
//texture
#define IMAGE_H				564
#define IMAGE_W				564

typedef struct 
{
	uint32_t scr_width;				// screen width
	uint32_t scr_height;			// screen hieght
	float	 scr_aspect_ratio;		// screen aspect ratio

	EGLDisplay display;				// parameters required to open diplay object
	EGLSurface surface;
	EGLContext context;
	EGL_DISPMANX_WINDOW_T nativewindow;

}display;

typedef struct
{
	GLfloat mvp[4][4];
	GLint f_text;
	GLint v_text;

}uniform;

typedef struct
{
	GLuint mvp;
	GLuint f_text;
	GLuint v_text;
	GLuint sampler;

}uniform_loc;

typedef struct
{
	GLfloat pos_image[3 * NUM_VERTICES];
	GLfloat texCoord_image[2 * NUM_VERTICES];
	GLfloat pos_text[NUM_MAX_TEXT * 6 * 3];
	GLfloat texCoord_text[NUM_MAX_TEXT * 6 * 2];

}attribute;

typedef struct
{
	GLuint pos;
	GLuint texCoord;

}attribute_loc;

typedef struct 
{
	GLushort indices[6];

	uniform			m_uniform;
	uniform_loc		m_uniform_loc;
	attribute		m_attribute;
	attribute_loc	m_attribute_loc;

	GLuint v_text;
	GLuint f_text;

}mesh;

typedef struct 
{
	GLuint m_program;
	GLuint m_shaders[NUM_SHADERS];

}shader;

typedef struct
{
	GLuint m_texture;
	unsigned char* data;
	int image_h;
	int image_w;
	texture_atlas_t* text_atlas;
	texture_font_t* text_font;

}texture;

typedef struct
{
	int next_image ;
	int prev_image ;
	int image_count;
	int usb_image_count;
	char file_name[16];
	FILE* file_ptr;
	FILE* conf_ptr;
	char file_info[54];
	int pixel_size;
	int pixel_offset; 

}Info;

typedef struct
{
	int char_len_1;
	wchar_t text_1[8];
	float base_1_x;
	float base_1_y;

}text;

typedef struct 
{
	int		need_drawing;
	float	version;

}general;

unsigned char test_data[] = {
255,127,0,
255,127,0,
255,127,0,
};

display	m_display;
mesh	m_mesh;
shader	m_shader;
texture	m_texture;
Info	m_info;
text	m_text;
general m_general;



inline void print_version()
{
	m_general.version = 1.3;
	printf(" Xray controller .\n Raspi Code\n Version:%2.1f\n By : Tanmay Patil\n ", m_general.version);
}



int init_display()
{
	int32_t			success = 0;
	EGLBoolean		result;
	EGLint			num_config;

	DISPMANX_ELEMENT_HANDLE_T	dispman_element;
	DISPMANX_DISPLAY_HANDLE_T	dispman_display;
	DISPMANX_UPDATE_HANDLE_T	dispman_update;
	VC_RECT_T					dst_rect;
	VC_RECT_T					src_rect;
	EGLConfig					config;

	//creates problem is func eglCreateContext
	static const EGLint attribute_list[] =
	{
		//EGL_COLOR_BUFFER_TYPE,	 	EGL_LUMINANCE_BUFFER, //EGL_RGB_BUFFER
		EGL_RED_SIZE,   			8,	// default is 0
		EGL_GREEN_SIZE, 			8,
		EGL_BLUE_SIZE,  			8,
		EGL_ALPHA_SIZE, 			8,
		//EGL_LUMINANCE_SIZE, 		8,
		EGL_SURFACE_TYPE, 			EGL_WINDOW_BIT,
		EGL_NONE
	};

	static const EGLint context_attributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	memset( &m_display, 0, sizeof(display) );

	bcm_host_init(); // must be called be4 any gpu function is called

	m_display.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (m_display.display==EGL_NO_DISPLAY)
	{
		printf("init_opengl : error in eglGetDisplay\n");
		exit(-1);
	}

	result = eglInitialize(m_display.display, NULL, NULL);
	if (EGL_FALSE == result)
	{
		printf("init_opengl : error in initializing EGL Display\n");
		exit(-1);
	}

	result = eglChooseConfig(m_display.display, attribute_list, &config, 17*sizeof(attribute_list[0]), &num_config);
	if (EGL_FALSE == result)
	{
		printf("init_opengl : error in getting egl frame buffer configuration\n");
		exit(-1);
	}

	result = eglBindAPI(EGL_OPENGL_ES_API);
	if (EGL_FALSE == result)
	{
		printf("init_opengl : error in binding api\n");
		exit(-1);
	}

	m_display.context = eglCreateContext(m_display.display, config, EGL_NO_CONTEXT, context_attributes);
	if (m_display.context==EGL_NO_CONTEXT)
	{
		printf("init_opengl : error in creating context . \n");
		exit(-1);
	}


	// create an EGL window surface (video core api)(bug 1920*1080 always)(only for rpi)
	success = graphics_get_display_size(0 /*display number*/, &m_display.scr_width, &m_display.scr_height);
	if ( success < 0 )
	{
		printf("init_opengl : error in creating EGL window surface\n");
		exit(-1);
	}

	if ( glGetError() != 0 )
	{
		printf("init_opengl : error - exit point 1.\n");
		exit(-1);
	}

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = m_display.scr_width;
	dst_rect.height = m_display.scr_height;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = m_display.scr_width << 16;
	src_rect.height = m_display.scr_height << 16;        

	dispman_display = vc_dispmanx_display_open( 0 /*display number*/);
	dispman_update = vc_dispmanx_update_start( 0 );

	dispman_element = 
	vc_dispmanx_element_add(dispman_update, dispman_display,
							0/*layer*/, &dst_rect, 0/*src*/,
							&src_rect, DISPMANX_PROTECTION_NONE, 
							0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

	m_display.nativewindow.element = dispman_element;
	m_display.nativewindow.width   = m_display.scr_width;
	m_display.nativewindow.height  = m_display.scr_height;
	vc_dispmanx_update_submit_sync( dispman_update );
	if ( glGetError() != 0 )
	{
		printf("init_opengl : error - exit point 2.\n");
		exit(-1);
	}

	m_display.surface = eglCreateWindowSurface( m_display.display, config, &m_display.nativewindow, NULL );
	if(m_display.surface == EGL_NO_SURFACE)
	{
		printf("init_opengl : error could not create window surface\n");
		exit(-1);
	}

	// connect the context to the surface
	result = eglMakeCurrent(m_display.display, m_display.surface, m_display.surface, m_display.context);
	if ( EGL_FALSE == result )
	{
		printf("init_opengl : error - could not connect context to surface\n");
		exit(-1);
	}
	if ( glGetError() != 0 )
	{
		printf("init_opengl : error - exit point 3.\n");
		exit(-1);
	}

	glClear( GL_COLOR_BUFFER_BIT );
	glViewport ( 0, 0, m_display.scr_width, m_display.scr_height ); 
	if ( glGetError() != 0 )
	{
		printf("init_opengl : error - exit point 4.\n");
		exit(-1);
	}

	m_display.scr_aspect_ratio = (float) ( ((float)m_display.scr_width) / ((float)m_display.scr_height) );
	glCullFace(GL_BACK);
	return(0);
}



void CheckShaderError(GLuint shader, GLuint flag, bool isProgram)
{
	GLint success = 0;
	GLint infoLen = 0;

	if(isProgram)
		glGetProgramiv(shader, flag, &success);
	else
		glGetShaderiv(shader, flag, &success);

	if(!success)
	{
		if(isProgram)
		{
			glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &infoLen );
			if ( infoLen > 1 )
			{
				char* infoLog = malloc (sizeof(char) * infoLen );
				glGetProgramInfoLog (shader, infoLen, NULL, infoLog );
				fprintf (stderr, "Error linking program:\n%s\n", infoLog );            
				free ( infoLog );
			}
			glDeleteProgram ( m_shader.m_program );
		}
		else
		{
			glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &infoLen );
			if ( infoLen > 1 )
			{
				char* infoLog = malloc (sizeof(char) * infoLen );
				glGetShaderInfoLog (shader, infoLen, NULL, infoLog );
				fprintf (stderr, "Error compiling shader:\n%s\n", infoLog );            
				free ( infoLog );
			}
			glDeleteProgram ( m_shader.m_program );
		}

		printf("CheckShaderError.\n");
		exit(-1);
	}
}

int init_shaders()
{
	GLchar vShaderStr[] =  
		"attribute vec3 position;								\n"
		"attribute vec2 texCoord;								\n"
		"varying vec2 texCoord0;								\n"
		"uniform mat4 MVP;										\n"
		"uniform int v_text;									\n"
		"void main()											\n"
		"{														\n"
		"	texCoord0 = texCoord;								\n"
		"	if(v_text == 0)										\n"
		"		{gl_Position = vec4(position, 1.0);}			\n"
		"	else 												\n"
		"		{gl_Position = MVP * vec4(position, 1.0);}		\n"
		"}														\n";
	
	GLchar fShaderStr[] =
		"varying vec2 texCoord0;								\n"
		"uniform sampler2D sampler;								\n"
		"uniform int f_text;									\n"
		"void main()											\n"
		"{														\n"
		"	if(f_text == 0)										\n"
		"		{gl_FragColor = vec4(texture2D(sampler,texCoord0).rgb,1.0);}\n"
		"	else 												\n"
		"		{gl_FragColor = vec4(0.9,0.9,0.9,texture2D(sampler,texCoord0).a);}			\n"
		"}														\n";

	const GLchar* vShaderPtr = vShaderStr;
	const GLchar* fShaderPtr = fShaderStr;

	memset( &m_shader, 0, sizeof(shader) );

	m_shader.m_program = glCreateProgram();

	m_shader.m_shaders[0] = glCreateShader(GL_VERTEX_SHADER);
	m_shader.m_shaders[1] = glCreateShader(GL_FRAGMENT_SHADER);
	if(m_shader.m_shaders[0] == 0)
	{
		printf("Error compiling VRETEX shader.\n");
		glDeleteShader(m_shader.m_shaders[0]);
		glDeleteShader(m_shader.m_shaders[1]);
		exit(-1);
	}
	else if(m_shader.m_shaders[1] == 0)
	{
		printf("Error compiling FRAGMENT shader.\n");
		glDeleteShader(m_shader.m_shaders[0]);
		glDeleteShader(m_shader.m_shaders[1]);
		exit(-1);
	}

	glShaderSource(m_shader.m_shaders[0], 1, &vShaderPtr, NULL);
	glCompileShader(m_shader.m_shaders[0]);
	CheckShaderError(m_shader.m_shaders[0], GL_COMPILE_STATUS, false);

	glShaderSource(m_shader.m_shaders[1], 1, &fShaderPtr, NULL);
	glCompileShader(m_shader.m_shaders[1]);
	CheckShaderError(m_shader.m_shaders[1], GL_COMPILE_STATUS, false);

	glAttachShader (m_shader.m_program, m_shader.m_shaders[0]);
	glAttachShader (m_shader.m_program, m_shader.m_shaders[1]);

	glLinkProgram(m_shader.m_program);
	CheckShaderError(m_shader.m_program, GL_LINK_STATUS, true);

	glValidateProgram(m_shader.m_program);
	CheckShaderError(m_shader.m_program, GL_LINK_STATUS, true);

	glUseProgram(m_shader.m_program);

	return (0);
}

void shader_update_image(/*const Transform& transform, const Camera& camera*/)
{
	m_mesh.m_uniform.v_text = 0; 
	m_mesh.m_uniform.f_text = 0;

	glActiveTexture(GL_TEXTURE1); // testing
	glBindTexture(GL_TEXTURE_2D, m_texture.m_texture); // testing
	glUniform1i(m_mesh.m_uniform_loc.sampler, 1); //coz glActiveTexture(GL_TEXTURE0); in init_texture()

	glUniform1i(m_mesh.m_uniform_loc.v_text, m_mesh.m_uniform.v_text);
	glUniform1i(m_mesh.m_uniform_loc.f_text, m_mesh.m_uniform.f_text);
	glUniformMatrix4fv( m_mesh.m_uniform_loc.mvp, 1, GL_FALSE, (GLfloat *) m_mesh.m_uniform.mvp); //edit matrix for image rendering purpose

	glVertexAttribPointer ( m_mesh.m_attribute_loc.pos, 3, GL_FLOAT, GL_FALSE, 0, &m_mesh.m_attribute.pos_image[0]);
	glEnableVertexAttribArray ( m_mesh.m_attribute_loc.pos );
	glVertexAttribPointer ( m_mesh.m_attribute_loc.texCoord, 2, GL_FLOAT, GL_FALSE, 0, &m_mesh.m_attribute.texCoord_image[0]);
	glEnableVertexAttribArray ( m_mesh.m_attribute_loc.texCoord );
}

void shader_update_text(/*const Transform& transform, const Camera& camera*/)
{
	m_mesh.m_uniform.v_text = 1;
	m_mesh.m_uniform.f_text = 1;

	glActiveTexture(GL_TEXTURE0);							//test if needed or not - testing
	glBindTexture(GL_TEXTURE_2D, m_texture.text_atlas->id); //test if needed or not - testing
	glUniform1i ( m_mesh.m_uniform_loc.sampler, 0); //coz glActiveTexture(GL_TEXTURE1); in init_texture()

	glUniform1i( m_mesh.m_uniform_loc.v_text, m_mesh.m_uniform.v_text);
	glUniform1i( m_mesh.m_uniform_loc.f_text, m_mesh.m_uniform.f_text);
	glUniformMatrix4fv( m_mesh.m_uniform_loc.mvp, 1, GL_FALSE, (GLfloat *) m_mesh.m_uniform.mvp);

	glVertexAttribPointer ( m_mesh.m_attribute_loc.pos, 3, GL_FLOAT, GL_FALSE, 0, &m_mesh.m_attribute.pos_text[0]);
	glEnableVertexAttribArray ( m_mesh.m_attribute_loc.pos );
	glVertexAttribPointer ( m_mesh.m_attribute_loc.texCoord, 2, GL_FLOAT, GL_FALSE, 0, &m_mesh.m_attribute.texCoord_text[0]);
	glEnableVertexAttribArray ( m_mesh.m_attribute_loc.texCoord );
}



void mesh_init_vertices_image()
{
	m_mesh.m_attribute.pos_image[0] =-1;	m_mesh.m_attribute.pos_image[1] =-1;	m_mesh.m_attribute.pos_image[2] = 0;
	m_mesh.m_attribute.texCoord_image[0] = 0;	m_mesh.m_attribute.texCoord_image[1] = 0;

	m_mesh.m_attribute.pos_image[3] =-1;	m_mesh.m_attribute.pos_image[4] = 1;	m_mesh.m_attribute.pos_image[5] = 0;
	m_mesh.m_attribute.texCoord_image[2] = 0;	m_mesh.m_attribute.texCoord_image[3] = 1;
	
	m_mesh.m_attribute.pos_image[6] = 1;	m_mesh.m_attribute.pos_image[7] = 1;	m_mesh.m_attribute.pos_image[8] = 0; 
	m_mesh.m_attribute.texCoord_image[4] = 1;	m_mesh.m_attribute.texCoord_image[5] = 1;
	
	m_mesh.m_attribute.pos_image[9] = 1;	m_mesh.m_attribute.pos_image[10] =-1;	m_mesh.m_attribute.pos_image[11] = 0;
	m_mesh.m_attribute.texCoord_image[6] = 1;	m_mesh.m_attribute.texCoord_image[7] = 0;

	m_mesh.indices[0] = 0;	m_mesh.indices[1] = 1;	m_mesh.indices[2] = 2;
	m_mesh.indices[3] = 0;	m_mesh.indices[4] = 2;	m_mesh.indices[5] = 3;
}

void mesh_init_vertices_text() 
{
	int i,j,k, kerning = 0;
	float x0,y0,x1,y1;
	float s0,t0,s1,t1;
	float base_x = m_text.base_1_x;
	float base_y = m_text.base_1_y;
	texture_glyph_t *glyph;
	m_text.char_len_1 = swprintf(m_text.text_1, 8, L"%d/%d", m_info.next_image, m_info.image_count);
	if(m_text.char_len_1 > 0)
	{
		for(i=0;i<m_text.char_len_1;i++)
		{
			glyph = texture_font_get_glyph(m_texture.text_font, m_text.text_1[i]);
			if( glyph != NULL )
			{
				kerning = 0;
				if( i > 0) //test if needed
				{kerning = texture_glyph_get_kerning( glyph, m_text.text_1[i-1] );}
				base_x += kerning;

				x0 = ( base_x + (float)glyph->offset_x );
				y0 = ( base_y + (float)glyph->offset_y );
				x1 = ( x0 + (float)glyph->width );
				y1 = ( y0 - (float)glyph->height );

				x0 = (2*x0)/m_display.scr_width;
				x1 = (2*x1)/m_display.scr_width;
				y0 = (2*y0)/m_display.scr_height;
				y1 = (2*y1)/m_display.scr_height;

				s0 = glyph->s0;
				t0 = glyph->t0;
				s1 = glyph->s1;
				t1 = glyph->t1;

				j=18*i;
				k=12*i;

				m_mesh.m_attribute.pos_text[j]=x0;		m_mesh.m_attribute.pos_text[j+1]=y0;		m_mesh.m_attribute.pos_text[j+2]=0.0;
				m_mesh.m_attribute.texCoord_text[k]=s0;			m_mesh.m_attribute.texCoord_text[k+1]=t0;
				m_mesh.m_attribute.pos_text[j+3]=x0;	m_mesh.m_attribute.pos_text[j+4]=y1;		m_mesh.m_attribute.pos_text[j+5]=0.0;
				m_mesh.m_attribute.texCoord_text[k+2]=s0;		m_mesh.m_attribute.texCoord_text[k+3]=t1;
				m_mesh.m_attribute.pos_text[j+6]=x1;	m_mesh.m_attribute.pos_text[j+7]=y1;		m_mesh.m_attribute.pos_text[j+8]=0.0;
				m_mesh.m_attribute.texCoord_text[k+4]=s1;		m_mesh.m_attribute.texCoord_text[k+5]=t1;
				m_mesh.m_attribute.pos_text[j+9]=x0;	m_mesh.m_attribute.pos_text[j+10]=y0;		m_mesh.m_attribute.pos_text[j+11]=0.0;
				m_mesh.m_attribute.texCoord_text[k+6]=s0;		m_mesh.m_attribute.texCoord_text[k+7]=t0;
				m_mesh.m_attribute.pos_text[j+12]=x1;	m_mesh.m_attribute.pos_text[j+13]=y1;		m_mesh.m_attribute.pos_text[j+14]=0.0;
				m_mesh.m_attribute.texCoord_text[k+8]=s1;		m_mesh.m_attribute.texCoord_text[k+9]=t1;
				m_mesh.m_attribute.pos_text[j+15]=x1;	m_mesh.m_attribute.pos_text[j+16]=y0;		m_mesh.m_attribute.pos_text[j+17]=0.0;
				m_mesh.m_attribute.texCoord_text[k+10]=s1;		m_mesh.m_attribute.texCoord_text[k+11]=t0;

				base_x += glyph->advance_x;

			}
		}
	}
}

int init_mesh()
{
	memset(&m_mesh, 0, sizeof(mesh));
	mesh_init_vertices_image();

	m_mesh.m_attribute_loc.pos		= glGetAttribLocation ( m_shader.m_program, "position" );
	m_mesh.m_attribute_loc.texCoord = glGetAttribLocation ( m_shader.m_program, "texCoord" );

	m_mesh.m_uniform_loc.mvp     = glGetUniformLocation ( m_shader.m_program, "MVP" );
	m_mesh.m_uniform_loc.v_text  = glGetUniformLocation ( m_shader.m_program, "v_text" );
	m_mesh.m_uniform_loc.f_text  = glGetUniformLocation ( m_shader.m_program, "f_text" );
	m_mesh.m_uniform_loc.sampler = glGetUniformLocation ( m_shader.m_program, "sampler" );

	shader_update_image();//coz we r not changing the cointaints of image attributes so call only once

	return (0);
}


int init_texture()
{
	memset( &m_texture, 0, sizeof(texture) );

	glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 ); //global
	m_texture.data = (unsigned char *)malloc( IMAGE_W * IMAGE_H * 1 );

	glActiveTexture(GL_TEXTURE0);
	m_texture.text_atlas = texture_atlas_new( 1024, 1024, 1 );
	m_texture.text_font = texture_font_new( m_texture.text_atlas, "./fonts/Vera.ttf", 60/*size*/ );

	texture_font_load_glyphs( m_texture.text_font,	L" !\"#$%&'()*+,-./0123456789:;<=>?"
													L"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
													L"`abcdefghijklmnopqrstuvwxyz{|}~");
	texture_atlas_upload(m_texture.text_atlas); //uploads data to gpu memory using glTexImage2D().


	glActiveTexture(GL_TEXTURE1);
	glGenTextures(1, &m_texture.m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture.m_texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // only for Active texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return (0);
}


void get_file_name()
{
	int count = m_info.next_image;
	m_info.file_name[10] = 48 + ( count % 10) ;
	count = count / 10;
	m_info.file_name[9] =  48 + ( count % 10) ;
	count = count / 10;
	m_info.file_name[8] =  48 + ( count % 10) ;
	count = count / 10;
	m_info.file_name[7] =  48 + ( count % 10) ;
}


void init_info()
{
	memset( &m_info, 0, sizeof(Info) );
	m_info.next_image = 0;
	m_info.prev_image = 0;
	memset( &m_mesh.m_uniform.mvp[0][0], 0, 16 * sizeof(GLfloat) );
	m_info.file_name[0] = 'P';
	m_info.file_name[1] = 'a';
	m_info.file_name[2] = 'r';
	m_info.file_name[3] = 'a';
	m_info.file_name[4] = 'd';
	m_info.file_name[5] = 'o';
	m_info.file_name[6] = 'X';
	m_info.file_name[7] = '0';
	m_info.file_name[8] = '0';
	m_info.file_name[9] = '0';
	m_info.file_name[10] = '0';
	m_info.file_name[11] = '.';
	m_info.file_name[12] = 'b';
	m_info.file_name[13] = 'm';
	m_info.file_name[14] = 'p';
	m_mesh.m_uniform.mvp[0][0] = 1.0f;//(float)m_display.scr_width;  //testing
	m_mesh.m_uniform.mvp[1][1] = 1.0f;//(float)m_display.scr_height; 
	m_mesh.m_uniform.mvp[2][2] = 1.0;
	m_mesh.m_uniform.mvp[3][3] = 1.0;
	m_text.base_1_x = -940;
	m_text.base_1_y = 480;
	m_info.conf_ptr = fopen("Xray.conf","r+");
	if(m_info.conf_ptr == NULL)
	{
		printf("Cannot open Xray.conf .\n");
		exit(-1);
	}
	fscanf(m_info.conf_ptr,"%d",&m_info.image_count);
	get_file_name();
	load_file();
	glClearColor(0.0, 0.0, 0.0, 1.0);						//black , alpha = 1 
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	//for text rendering
	glDisable(GL_BLEND);
}

int load_file()
{
	m_info.file_ptr = fopen( m_info.file_name, "rb" );
	fread( m_info.file_info, 54, 1, m_info.file_ptr );
	if(m_info.file_info[0] != 'B' && m_info.file_info[1] != 'M' )
	{
		fclose(m_info.file_ptr);
		printf("Error : File not BMP.\n");
		return (-1);
	}
	m_info.pixel_size = m_info.file_info[28];
	m_texture.image_w = m_info.file_info[18] + (m_info.file_info[19] << 8) ;
	m_texture.image_h = m_info.file_info[22] + (m_info.file_info[23] << 8) ;
	m_info.pixel_offset = m_info.file_info[10] + (m_info.file_info[11] << 8) ;
	if(m_texture.image_w != 564 && m_texture.image_w != 564 )
	{
		fclose(m_info.file_ptr);
		printf("Error : Image hieght or width not correct.\n");
		exit(-1);
	}
	fseek(m_info.file_ptr , m_info.pixel_offset, SEEK_SET);
	fread(m_texture.data, 564*564, 1, m_info.file_ptr);
	return (0);
}


void draw_image()
{
	glDisable(GL_BLEND);
	shader_update_image();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_texture.image_w, m_texture.image_h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, m_texture.data);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &m_mesh.indices[0]);
}

void draw_text()
{
	glEnable(GL_BLEND);
	mesh_init_vertices_text();
	shader_update_text();
	glDrawArrays(GL_TRIANGLES, 0, 6 * m_text.char_len_1);
}

void draw()
{
	draw_image();
	draw_text();
	eglSwapBuffers(m_display.display, m_display.surface);
}

void draw_loop()
{
	while(1)
	{
		m_general.need_drawing=1;
		if(m_general.need_drawing == 1)
		{
			//get_file_name();
			//load_file();
			draw();
		}
		usleep(10000); //10 milliseconds
	}
}




int main()
{
	init_display();
	if ( glGetError() != 0 )
	{
		printf("display : error\n");
		exit(-1);
	}

	init_shaders();
	if ( glGetError() != 0 )
	{
		printf("shader : error\n");
		exit(-1);
	}

	init_mesh();
	if ( glGetError() != 0 )
	{
		printf("mesh : error\n");
		exit(-1);
	}

	init_texture();
	if ( glGetError() != 0 )
	{
		printf("texture : error\n");
		exit(-1);
	}

	init_info();
	if ( glGetError() != 0 )
	{
		printf("init_info : error\n");
		exit(-1);
	}

	draw_loop();

	return (0);
}

/*
	notes:
	function : init_display()
		Everything remain the same except the  structs
		attribute_list[] : shall generate error in eglCreateContext
		context_attributes[] : no errors detected yet
		may need editing regarding screen aspect ratio problems.
	

	function : init_shaders()
		vShaderStr[] : is the vertex shader
		fShaderStr[] : is the fragment shader
			both can be edited as needed
	function : shader_update_image/text()
		updates gpu memory. i.e. updates all the uniforms and attributes
		edit if new attributes or uniforms are added.
		need to change matrix betw image n text render - for now no need we are not using matrix to draw image
		the cpu memory from where they are updated can also be changed.
		multiple shader_update() functions can be needed if we use different cpu memory to load from
	function : CheckShaderError()
		-


	function : init_mesh()
		gets all the uniform and attributes locations and calls shader_update().
		changes needed if more uniforms or attributes are added.
	function : mesh_init_vertices_image()
		inits the cpu memory from where gpu (uniform and attribute) memory is updated
		multiple mesh_init_vertices_image() function will be needed for different rendering elements(image , text , etc).

	

	function : init_texture()
		genetates texture. sets required texture parameters. binds the texture.needs extensive editing to make it text render compatible.
		also allocates data to read image files into from secondary memory. 

	

	function : init_info()
		inits structs.reads number of images from xray.conf . sets some opengl states.

	function : draw_loop()
		the function from where the program never exits.



	error testing:

	GLuint temp;
	temp =glGetError();
	if ( temp == GL_INVALID_ENUM )
		{
			printf("a\n");
			exit(-1);
		}
		if ( temp == GL_INVALID_VALUE )
		{
			printf("b\n");
			exit(-1);
		}
		if (temp != 0)
		{
			printf("some error here\n");
			exit(-1);
	}



	to do:

	1. how to interface ir remote .
	2. how to interface camera.
	3. how to produce image file in cpu memory from textures
	4. ssh setup
	5. screen aspect ratio problems
*/
