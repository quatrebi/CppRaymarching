#include <Windows.h>
#include <cmath>
#include <mutex>
#include <vector>
#include <numeric>
using std::vector;
using std::recursive_mutex;
using std::lock_guard;

const UINT8 width = 80;
const UINT8 height = 80;
const UINT8 pixelSize = 6;

HANDLE hwnd;
SMALL_RECT m_rectWindow;
CHAR_INFO* screen;

class Keyboard
{
public:

	struct KeyState
	{
		bool bPressed;
		bool bReleased;
		bool bHeld;
	};

	KeyState& operator[](UINT8 index) { return m_keys[index]; }

	void Update()
	{
		for (int i = 0; i < 256; i++)
		{
			m_keyNewState[i] = GetAsyncKeyState(i);

			m_keys[i].bPressed = false;
			m_keys[i].bReleased = false;

			if (m_keyNewState[i] != m_keyOldState[i])
			{
				if (m_keyNewState[i] & 0x8000)
				{
					m_keys[i].bPressed = !m_keys[i].bHeld;
					m_keys[i].bHeld = true;
				}
				else
				{
					m_keys[i].bReleased = true;
					m_keys[i].bHeld = false;
				}
			}

			m_keyOldState[i] = m_keyNewState[i];
		}
	}

private:
	short m_keyOldState[256] = { 0 };
	short m_keyNewState[256] = { 0 };
	KeyState m_keys[256];

} keys;


bool Error(LPCWSTR e)
{
	SetConsoleTitle(e);
	return FALSE;
}

bool init()
{
	hwnd = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hwnd == INVALID_HANDLE_VALUE)
		return Error(L"Bad Handle");

	screen = new CHAR_INFO[width * height];

	m_rectWindow = { 0, 0, 1, 1 };
	SetConsoleWindowInfo(hwnd, TRUE, &m_rectWindow);

	COORD coord = { (short)width, (short)height };
	if (!SetConsoleScreenBufferSize(hwnd, coord))
		Error(L"SetConsoleScreenBufferSize");

	// Assign screen buffer to the console
	if (!SetConsoleActiveScreenBuffer(hwnd))
		return Error(L"SetConsoleActiveScreenBuffer");

	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(cfi);
	cfi.nFont = 0;
	cfi.dwFontSize.X = pixelSize;
	cfi.dwFontSize.Y = pixelSize;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;

	wcscpy_s(cfi.FaceName, L"Consolas");
	if (!SetCurrentConsoleFontEx(hwnd, false, &cfi))
		return Error(L"SetCurrentConsoleFontEx");

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hwnd, &csbi))
		return Error(L"GetConsoleScreenBufferInfo");
	if (height > csbi.dwMaximumWindowSize.Y)
		return Error(L"Screen Height / Font Height Too Big");
	if (width > csbi.dwMaximumWindowSize.X)
		return Error(L"Screen Width / Font Width Too Big");

	m_rectWindow = { 0, 0, (short)width - 1, (short)height - 1 };
	if (!SetConsoleWindowInfo(hwnd, TRUE, &m_rectWindow))
		return Error(L"SetConsoleWindowInfo");

	return TRUE;
}

//recursive_mutex renderLock;

void Draw(UINT8 x, UINT8 y, short c = 0x2588, short col = 0x000F)
{
	//lock_guard<recursive_mutex> locker(renderLock);

	if (x >= 0 && x < width && y >= 0 && y < height)
	{
		screen[y * width + x].Char.UnicodeChar = c;
		screen[y * width + x].Attributes = col;
	}
}

#define clamp(x, mn, mx) min(max(x, mn), mx)

double map(double value, double start1, double stop1, double start2, double stop2)
{
	return ((value - start1) / (stop1 - start1)) * (stop2 - start2) + start2;
}

vector<double> mix(vector<double> x, vector<double> y, vector<double> a)
{
	vector<double> temp;
	for (size_t i = 0; i < x.size(); i++)
		temp.push_back(x[i] * (1 - a[i]) + y[i] * a[i]);
	return temp;
}

const double PI = 3.14159265359;
const double DEG_TO_RAD = PI / 180.0;

vector<double> rotateX(vector<double> vec, double angle)
{
	double c = cos(angle);
	double s = sin(angle);
	return { vec[0], vec[1] * c - vec[2] * s, vec[2] * c + vec[1] * s };
}

vector<double> rotateY(vector<double> vec, double angle)
{
	double c = cos(angle);
	double s = sin(angle);
	return { vec[0] * c + vec[2] * s, vec[1], vec[2] * c - vec[1] * s };
}

vector<double> rotateZ(vector<double> vec, double angle)
{
	double c = cos(angle);
	double s = sin(angle);
	return { vec[0] * c - vec[1] * s, vec[1] * c + vec[0] * s, vec[2] };
}

double dot(vector<double> vecA, vector<double> vecB)
{
	double temp = 0;
	for (size_t i = 0; i < vecA.size(); i++) temp += vecA[i] * vecB[i];
	return temp;
}

double length(vector<double> vec)
{
	return sqrt(dot(vec, vec));
}

vector<double> normalize(vector<double> vec)
{
	double len = length(vec);
	for (size_t i = 0; i < vec.size(); i++)
		vec[i] /= len;
	return vec;
}

vector<double> reflect(vector<double> I, vector<double> N)
{
	vector<double> temp; temp.resize(3);
	for (size_t i = 0; i < I.size(); i++)
		temp[i] = I[i] + 2.0 * dot(N, I) * N[i];
	return temp;
}

vector<double> fresnel(vector<double> F0, vector<double> h, vector<double> l)
{
	vector<double> temp;
	for (size_t i = 0; i < F0.size(); i++)
		temp.push_back(F0[i] + (1.0 - F0[i]) * pow(clamp(1.0 - dot(h, l), 0.0, 1.0), 5.0));
	return temp;
}

#define intersectSDF(x, y) max(x, y)
#define unionSDF(x, y) min(x, y)
#define differenceSDF(x, y) max(x, -y)

double planeSDF(vector<double> p) {
	return p[1];
}

double sphereSDF(vector<double>& p, vector<double>& sph)
{
	vector<double> temp = { p[0] - sph[0], p[1] - sph[1], p[2] - sph[2] };
	return length(temp) - sph[3];
}

double cubeSDF(vector<double> p, vector<double> cube) {
	vector<double> q = {abs(p[0] - cube[0]) - cube[3], abs(p[1] - cube[1]) - cube[3], abs(p[2] - cube[2]) - cube[3] };
	q = max(q, vector<double>(p.size(), 0));
	for (size_t i = 0; i < q.size(); i++)
		q[i] += min(max(q[0], max(q[1], q[2])), 0.0);
	return length(q);
}

#define MAX_MARCHING_STEPS 100
#define MIN_DIST 0.0
#define MAX_DIST 20.0
#define EPSILON 0.0001


int c_shades[] = { 0x00, 0x08, 0x07, 0x88, 0x87, 0x8F, 0x77, 0x7F, 0xFF };
int s_shades[3] = { 0x2591, 0x2592, 0x2593 };
short color, symbol;
vector<double> ro = { 0.0, 0.0, 5.0 };

vector<double> shading(vector<double> pos, vector<double> n, vector<double> rd, vector<double> ro)
{
	double shininess = 8.0, ks = 0.5, kd = 1.0;
	vector<double> _color; _color.resize(3);

	vector<double> ref = reflect(rd, n);

	{
		vector<double> light_pos = { -3, 7, 3.0 };
		vector<double> light_color = { 1.0, 0.7, 0.7 };

		vector<double> vl = normalize({ light_pos[0] - pos[0], light_pos[1] - pos[1], light_pos[2] - pos[2] });
		
		vector<double> diffuse;
		for (size_t i = 0; i < _color.size(); i++)
			diffuse.push_back(kd * max(0.0, dot(vl, n)));

		vector<double> specular;
		for (size_t i = 0; i < _color.size(); i++)
			specular.push_back(pow(max(0.0, dot(vl, ref)), shininess));

		vector<double> F = fresnel({ ks, ks, ks }, normalize({ vl[0] - rd[0], vl[1] - rd[1], vl[2] - rd[2] }), vl);

		for (size_t i = 0; i < _color.size(); i++)
			_color[i] += light_color[i] * mix(diffuse, specular, F)[i];
	}

	//{
	//	vector<double> light_pos = { -20.0, -20.0, -30.0 };
	//	vector<double> light_color = { 0.5, 0.7, 1.0 };

	//	vector<double> vl = normalize({ light_pos[0] - pos[0], light_pos[1] - pos[1], light_pos[2] - pos[2] });

	//	vector<double> diffuse;
	//	for (size_t i = 0; i < _color.size(); i++)
	//		diffuse.push_back(kd * max(0.0, dot(vl, n)));

	//	vector<double> specular;
	//	for (size_t i = 0; i < _color.size(); i++)
	//		specular.push_back(pow(max(0.0, dot(vl, ref)), shininess));

	//	vector<double> F = fresnel({ ks, ks, ks }, normalize({ vl[0] - rd[0], vl[1] - rd[1], vl[2] - rd[2] }), vl);

	//	for (size_t i = 0; i < _color.size(); i++)
	//		_color[i] += light_color[i] * mix(diffuse, specular, F)[i];
	//}

	//for (size_t i = 0; i < _color.size(); i++)
	//	_color[i] += fresnel({ ks, ks, ks }, n, { -rd[0], -rd[1], -rd[2] })[i];

	return _color;
}

static double max_grey;

double rayMarch(vector<double> ro, vector<double> rd, double start, double end)
{
	double depth = start;
	for (int i = 0; i < MAX_MARCHING_STEPS; i++)
	{
		vector<double> pos = { ro[0] + depth * rd[0], ro[1] + depth * rd[1], ro[2] + depth * rd[2] };
		vector<double> sphere = { 0.0, 0.0, 0.0, 1.0 };
		vector<double> sphere2 = { 0.5, 1.0, 0.0, 0.5 };
		double dist = unionSDF(sphereSDF(pos, sphere), sphereSDF(pos, sphere2));
		if (dist < EPSILON)
		{
			vector<double> n(pos);
			for (size_t i = 0; i < n.size(); i++)
				n[i] -= sphere[i], n[i] /= sphere[3];
			vector<double> rgb = shading(pos, n, rd, ro);
			for (size_t i = 0; i < rgb.size(); i++)
				rgb[i] = pow(rgb[i], 1.0 / 1.1);
			double grey = std::accumulate(rgb.begin(), rgb.end(), 0.0) / 3.0;

			color = c_shades[(int)map(grey, 0, (max_grey / 2, max_grey = max(grey, max_grey)), 0, 9)];
			symbol = s_shades[(int)map(grey, 0, (max_grey / 2, max_grey = max(grey, max_grey)), 0, 3)];


			// RAYCASTING? LIGHTING ALGORITHM
			//vector<double> l = { light[0] - pos[0], light[1] - pos[1], light[2] - pos[2] };
			//for (size_t i = 0; i < l.size(); i++)
			//	l[i] /= normalize(l)[i];
			//color = shades[(int)map(dot(n, l), 0, MAX_DIST * 3, 0, sizeof(shades) / sizeof(int))];


			return depth;
		}
		depth += dist;
		if (depth >= end) return end;
	}
	return end;
}


double angleZ;

void render(UINT8 x, UINT8 y, UINT8 w, UINT8 h)
{
	//lock_guard<recursive_mutex> locker(renderLock);

	for (UINT8 i = x; i < w; i++)
	{
		for (UINT8 j = y; j < h; j++)
		{
			vector<double> uv = { map(i, 0, width, -1.0, 1.0) , map(j, 0, height, -1.0, 1.0) };
			vector<double> pPos = { uv[0], uv[1], 2.0 };
			vector<double> rd = normalize({ pPos[0] - ro[0], pPos[1] - ro[1], pPos[2] - ro[2] });

			ro = rotateZ(ro, angleZ * DEG_TO_RAD);
			rd = rotateZ(rd, angleZ * DEG_TO_RAD);

			double dist = rayMarch(ro, rd, MIN_DIST, MAX_DIST);
			Draw(i, j, symbol, (dist > MAX_DIST - EPSILON ? 0x00 : color));
		}
	}
	WriteConsoleOutput(hwnd, screen, { (short)width, (short)height }, { 0, 0 }, &m_rectWindow);
}

#include <chrono>
#include <thread>
using std::thread;

int main()
{
	if (!init()) return EXIT_FAILURE;

	auto tp1 = std::chrono::system_clock::now();
	auto tp2 = std::chrono::system_clock::now();

	//vector<thread> ts = vector<thread>();
	//ts.push_back(thread(render, 0, 0, 60, 60));
	//ts.push_back(thread(render, 30,0, 60, 60));
	//ts.push_back(thread(render,  0, 30, 30, 60));
	//ts.push_back(thread(render, 30, 30, 60, 60));
	//ts.push_back(thread(render, 30,  0, 60, 30));

	while (!keys[VK_ESCAPE].bReleased)
	{
		tp2 = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsedTime = tp2 - tp1;
		tp1 = tp2;
		float fElapsedTime = elapsedTime.count();

		keys.Update();
		if (keys['Q'].bPressed || keys['Q'].bHeld) angleZ--;
		if (keys['E'].bPressed || keys['E'].bHeld) angleZ++;
		
		//for (size_t i = 0; i < ts.size(); i++)
		//	ts[i].join();

		render(0, 0, width, height);

		wchar_t s[256];
		swprintf_s(s, 256, L"RM - FPS: %3.2f | Angle %2.2f deg", 1.0f / fElapsedTime, angleZ);
		SetConsoleTitle(s);
	}
}