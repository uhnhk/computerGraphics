#include "Render.h"
#include <Windows.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "GUItextRectangle.h"





#ifdef _DEBUG
#include <Debugapi.h> 
struct debug_print
{
	template<class C>
	debug_print& operator<<(const C& a)
	{
		OutputDebugStringA((std::stringstream() << a).str().c_str());
		return *this;
	}
} debout;
#else
struct debug_print
{
	template<class C>
	debug_print& operator<<(const C& a)
	{
		return *this;
	}
} debout;
#endif

//библиотека для разгрузки изображений
//https://github.com/nothings/stb
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = false;


//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL *sender, KeyEventArg arg)
{
	//конвертируем код клавиши в букву
	auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

	switch (key)
	{
	case 'L':
		lightning = !lightning;
		break;
	case 'T':
		texturing = !texturing;
		break;
	case 'A':
		alpha = !alpha;
		break;
	}
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером
void initRender()
{
	//==============НАСТРОЙКА ТЕКСТУР================
	//4 байта на хранение пикселя
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	//просим сгенерировать нам Id для текстуры
	//и положить его в texId
	glGenTextures(1, &texId);

	//делаем текущую текстуру активной
	//все, что ниже будет применено texId текстуре.
	glBindTexture(GL_TEXTURE_2D, texId);


	int x, y, n;

	//загружаем картинку
	//см. #include "stb_image.h" 
	unsigned char* data = stbi_load("texture.png", &x, &y, &n, 4);
	//x - ширина изображения
	//y - высота изображения
	//n - количество каналов
	//4 - нужное нам количество каналов
	//пиксели будут хранится в памяти [R-G-B-A]-[R-G-B-A]-[..... 
	// по 4 байта на пиксель - по байту на канал
	//пустые каналы будут равны 255

	//Картинка хранится в памяти перевернутой 
	//так как ее начало в левом верхнем углу
	//по этому мы ее переворачиваем -
	//меняем первую строку с последней,
	//вторую с предпоследней, и.т.д.
	unsigned char* _tmp = new unsigned char[x * 4]; //времянка
	for (int i = 0; i < y / 2; ++i)
	{
		std::memcpy(_tmp, data + i * x * 4, x * 4);//переносим строку i в времянку
		std::memcpy(data + i * x * 4, data + (y - 1 - i) * x * 4, x * 4); //(y-1-i)я строка -> iя строка
		std::memcpy(data + (y - 1 - i) * x * 4, _tmp, x * 4); //времянка -> (y-1-i)я строка
	}
	delete[] _tmp;


	//загрузка изображения в видеопамять
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	//выгрузка изображения из опперативной памяти
	stbi_image_free(data);


	//настройка режима наложения текстур
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
												  //GL_REPLACE -- полная замена политога текстурой
	//настройка тайлинга
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//настройка фильтрации
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//======================================================

	//================НАСТРОЙКА КАМЕРЫ======================
	camera.caclulateCameraPos();

	//привязываем камеру к событиям "движка"
	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
	//==============НАСТРОЙКА СВЕТА===========================
	//привязываем свет к событиям "движка"
	gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
	gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
	gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
	//========================================================
	//====================Прочее==============================
	gl.KeyDownEvent.reaction(switchModes);
	text.setSize(512, 180);
	//========================================================
	   

	camera.setPosition(2, 1.5, 1.5);
}

//функция для вычисление нормали
double* normal(double* A, double* B, double* C, int size)
{

	double* u1 = new double[size];
	double* u2 = new double[size];

	for (int i = 0; i < size; i++)
	{
		u1[i] = B[i] - A[i];
		u2[i] = C[i] - A[i];
	}
	/* cout << u1[0] << "   " << u1[1] << "     " << u1[2];
	 cout << u2[0] << "   " << u2[1] << "     " << u2[2];*/


	double* norma = new double[3];

	norma[0] = u1[1] * u2[2] - u1[2] * u2[1];
	norma[1] = u1[2] * u2[0] - u1[0] * u2[2];
	norma[2] = u1[0] * u2[1] - u1[1] * u2[0];

	double dlina = sqrt((norma[0] * norma[0]) + (norma[1] * norma[1]) + (norma[2] * norma[2]));
	//cout << norma[0] << "   " << norma[1] << "     " << norma[2];

	if (dlina != 0) {
		norma[0] /= -dlina;
		norma[1] /= -dlina;
		norma[2] /= -dlina;
	}


	delete[] u1;
	delete[] u2;

	return norma;
}
//конец функции для вычисления нормали

void Render(double delta_time)
{    
	glEnable(GL_DEPTH_TEST);
	
	//настройка камеры и света
	//в этих функциях находятся OGLные функции
	//которые устанавливают параметры источника света
	//и моделвью матрицу, связанные с камерой.

	if (gl.isKeyPressed('F')) //если нажата F - свет из камеры
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	light.SetUpLight();


	//рисуем оси
	gl.DrawAxes();

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	

	//включаем режимы, в зависимости от нажания клавиш. см void switchModes(OpenGL *sender, KeyEventArg arg)
	if (lightning)
		glEnable(GL_LIGHTING);
	if (texturing)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0); //сбрасываем текущую текстуру
	}
		
	if (alpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
		
	//=============НАСТРОЙКА МАТЕРИАЛА==============


	//настройка материала, все что рисуется ниже будет иметь этот метериал.
	//массивы с настройками материала
	float amb[] = { 0.05f, 0.05f, 0.05f, 1.0f };  
	float dif[] = { 0.6f, 0.3f, 0.1f, 1.0f };     
	float spec[] = { 0.6f, 0.3f, 0.1f, 1.0f };     
	float sh = 30.0f;

	//фоновая
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//дифузная
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//зеркальная
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec); 
	//размер блика
	glMaterialf(GL_FRONT, GL_SHININESS, sh);

	//чтоб было красиво, без квадратиков (сглаживание освещения)
	glShadeModel(GL_SMOOTH); //закраска по Гуро      
			   //(GL_SMOOTH - плоская закраска)

	//============ РИСОВАТЬ ТУТ ==============
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glBindTexture(GL_TEXTURE_2D, texId);
	glBegin(GL_QUADS);
	double A[]{ 0, 0, 0 };
	double A1[]{ 0, 0, 15 };

	double B[]{ 7, 5, 0 };
	double B1[]{ 7, 5, 15 };

	double C[]{ 1, 7, 0 };
	double C1[]{ 1, 7, 15 };

	double D[]{ -2, 3, 0 };
	double D1[]{ -2, 3, 15 };

	double E[]{ -7, 6, 0 };
	double E1[]{ -7, 6, 15 };

	double F[]{ -4, 2, 0 };
	double F1[]{ -4, 2, 15 };

	double G[]{ -8, -5, 0 };
	double G1[]{ -8, -5, 15 };

	double H[]{ 1, -7, 0 };
	double H1[]{ 1, -7, 15 };

	//дно
	glBegin(GL_QUADS);
	glNormal3d(0, 0, -1);
	glColor3d(1, 0, 0);
	glVertex3dv(A);
	glVertex3dv(B);
	glVertex3dv(C);
	glVertex3dv(D);

	glColor3d(0.9, 0.1, 0);
	glVertex3dv(A);
	glVertex3dv(D);
	glVertex3dv(E);
	glVertex3dv(F);

	glColor3d(0.8, 0.3, 0.1);
	glVertex3dv(A);
	glVertex3dv(F);
	glVertex3dv(G);
	glVertex3dv(H);
	//конец дна

	//стены
	double* norm1 = normal(B1, B, A, 3);
	glNormal3dv(norm1);
	glColor3d(0.6, 0.6, 0.2);
	glVertex3dv(A);
	glVertex3dv(B);
	glVertex3dv(B1);
	glVertex3dv(A1);

	double* norm2 = normal(C1, C, B, 3);
	glNormal3dv(norm2);
	glColor3d(0.4, 0.8, 0.4);
	glVertex3dv(B);
	glVertex3dv(C);
	glVertex3dv(C1);
	glVertex3dv(B1);

	double* norm3 = normal(D1, D, C, 3);
	glNormal3dv(norm3);
	glColor3d(0.1, 1, 0.6);
	glVertex3dv(C);
	glVertex3dv(D);
	glVertex3dv(D1);
	glVertex3dv(C1);

	double* norm4 = normal(E1, E, D, 3);
	glNormal3dv(norm4);
	glColor3d(0.1, 0.7, 0.8);
	glVertex3dv(D);
	glVertex3dv(E);
	glVertex3dv(E1);
	glVertex3dv(D1);

	double* norm5 = normal(F1, F, D, 3);
	glNormal3dv(norm5);
	glColor3d(0.5, 0.7, 0.9);
	glVertex3dv(E);
	glVertex3dv(F);
	glVertex3dv(F1);
	glVertex3dv(E1);

	double* norm6 = normal(G1, G, F, 3);
	glNormal3dv(norm6);
	glColor3d(0.9, 0.7, 0.5);
	glVertex3dv(F);
	glVertex3dv(G);
	glVertex3dv(G1);
	glVertex3dv(F1);

	double* norm7 = normal(G1, H, G, 3);
	glNormal3dv(norm7);
	glColor3d(0.5, 0.3, 0.1);
	glVertex3dv(G);
	glVertex3dv(H);
	glVertex3dv(H1);
	glVertex3dv(G1);

	double* norm8 = normal(A1, A, H, 3);
	glNormal3dv(norm8);
	glColor3d(0.6, 0.6, 0.6);
	glVertex3dv(H);
	glVertex3dv(A);
	glVertex3dv(A1);
	glVertex3dv(H1);
	//конец стен

	//крышка
	glBindTexture(GL_TEXTURE_2D, texId);
	glNormal3d(0, 0, 1);
	glColor3d(0.4, 0.1, 0.9);
	glVertex3dv(A1);
	glVertex3dv(B1);
	glVertex3dv(C1);
	glVertex3dv(D1);

	glColor3d(0.7, 0.8, 0.3);
	glVertex3dv(A1);
	glVertex3dv(D1);
	glVertex3dv(E1);
	glVertex3dv(F1);

	glColor3d(0.3, 0.4, 0.6);
	glVertex3dv(A1);
	glVertex3dv(F1);
	glVertex3dv(G1);
	glVertex3dv(H1);
	glEnd();
	//конец крышки


	glBegin(GL_LINES);
	//нормаль для дна
	glVertex3d(-2, 0, 0);
	glVertex3d(-2, 0, -5);

	//нормаль для крышки
	glVertex3d(-2, 0, 10);
	glVertex3d(-2, 0, 15);
	glEnd();
	

	//===============================================

	//рисуем источник света
	light.DrawLightGizmo();

	//================Сообщение в верхнем левом углу=======================

	//переключаемся на матрицу проекции
	glMatrixMode(GL_PROJECTION);
	//сохраняем текущую матрицу проекции с перспективным преобразованием
	glPushMatrix();
	//загружаем единичную матрицу в матрицу проекции
	glLoadIdentity();

	//устанавливаем матрицу паралельной проекции
	glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

	//переключаемся на моделвью матрицу
	glMatrixMode(GL_MODELVIEW);
	//сохраняем матрицу
	glPushMatrix();
    //сбразываем все трансформации и настройки камеры загрузкой единичной матрицы
	glLoadIdentity();

	//отрисованное тут будет визуалзироватся в 2д системе координат
	//нижний левый угол окна - точка (0,0)
	//верхний правый угол (ширина_окна - 1, высота_окна - 1)

	
	std::wstringstream ss;
	ss << std::fixed << std::setprecision(3);
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
	ss << L"Коорд. света: (" << std::setw(7) <<  light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5)<< delta_time << std::endl;

	
	text.setPosition(10, gl.getHeight() - 10 - 180);
	text.setText(ss.str().c_str());
	text.Draw();

	//восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	

}   



