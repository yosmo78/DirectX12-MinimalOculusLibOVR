//https://developer.oculus.com/documentation/native/pc/dg-libovr/
//https://developer.oculus.com/reference/libovr/1.43/o_v_r_c_a_p_i_8h/
//https://developer.oculus.com/documentation/native/pc/gsg-intro-oneworld/#gsg-intro-oneworld
//https://github.com/jherico/OculusSDK/blob/master/LibOVR/Include/OVR_ErrorCode.h

//TODO what is difference between OculusDLL and libovr?

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

//windows
#include <windows.h>

#if !_M_X64
#error 64-BIT platform required!
#endif

//direct x
#include <d3d12.h>
#include <dxgi1_4.h>  //how low can we drop this...

#if MAIN_DEBUG
#include "vertShaderDebug.h" //in debug use .cso files for hot shader reloading for faster developing
#include "pixelShaderDebug.h"
#else
#include "vertShader.h"
#include "pixelShader.h"
#endif

#include <stdint.h>
#include <math.h>
#if MAIN_DEBUG
#include <stdio.h>
#include <assert.h>
#endif
#include <stdlib.h> //if switching to mainCRT would have to replace with our own allocator, which is fine just use virtual alloc

// for struct references look in OVR_CAPI.h and 
#include "OVR_CAPI_D3D.h"

#define PI_F 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679f
#define PI_D 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32; //floating 32
typedef double   f64; //floating 64

typedef struct Mat3f
{
	union
	{
		f32 m[3][3];
	};
} Mat3f;

typedef struct Mat4f
{
	union
	{
		f32 m[4][4];
	};
} Mat4f;

typedef struct Mat3x4f
{
	union
	{
		f32 m[3][4];
	};
} Mat3x4f;

typedef struct Vec2f
{
	union
	{
		f32 v[2];
		struct
		{
			f32 x;
			f32 y;
		};
	};
} Vec2f;

typedef struct Vec3f
{
	union
	{
		f32 v[3];
		struct
		{
			f32 x;
			f32 y;
			f32 z;
		};
	};
} Vec3f;

typedef struct Vec4f
{
	union
	{
		f32 v[4];
		struct
		{
			f32 x;
			f32 y;
			f32 z;
			f32 w;
		};
	};
} Vec4f;

typedef struct Quatf
{
	union
	{
		f32 q[4];
		struct
		{
			f32 w; //real
			f32 x;
			f32 y;
			f32 z;
		};
		struct
		{
			f32 real; //real;
			Vec3f v;
		};
	};
} Quatf;


typedef struct vertexShaderCB
{
	Mat4f mvpMat;
	Mat3x4f nMat; //there is 3 floats of padding for 16 byte alignment;
} vertexShaderCB;

typedef struct pixelShaderCB
{
	Vec4f vLightColor;
	Vec3f vInvLightDir; //there is 3 floats of padding for 16 byte alignment;
} pixelShaderCB;


//Game state
u8 Running;
u8 isPaused;


//Oculus Globals
u64 oculusFrameIndex;
s32 oculusNUM_FRAMES;
ovrSession oculusSession; //oculus's global state variable
ovrGraphicsLuid oculusGLuid; //uid of graphics card that has headset attachted to it
ovrHmdDesc oculusHMDDesc; //description of the headset
ovrRecti oculusEyeRenderViewport[ovrEye_Count]; //TODO do I even need to store this!
D3D12_VIEWPORT EyeViewports[ovrEye_Count];
D3D12_RECT EyeScissorRects[ovrEye_Count];
ovrTextureSwapChain oculusEyeSwapChains[ovrEye_Count];
ID3D12Resource** oculusEyeBackBuffers;
ID3D12Resource* depthStencilBuffers[ovrEye_Count];

D3D12_CPU_DESCRIPTOR_HANDLE eyeStartingRTVHandle[ovrEye_Count];
D3D12_CPU_DESCRIPTOR_HANDLE eyeDSVHandle[ovrEye_Count];

//DirectX12 Globals
const u8 numSwapChains = 2; // we should allow users the ability to display the game on their screen, so change to 3 (1 for left eye, 1 for right eye, 1 for toggleable render window(when not displaying on screen a very small check box window is appearing saying toggle to render to screen too, then in options in game you can untoggle and turn it off))
ID3D12Device* device;
ID3D12CommandQueue* commandQueue;
ID3D12CommandAllocator** commandAllocators;
ID3D12GraphicsCommandList* commandLists[ovrEye_Count+1]; //one extra for model uploading, would be used for streaming!

//views
D3D12_VERTEX_BUFFER_VIEW planeVertexBufferView;
D3D12_INDEX_BUFFER_VIEW planeIndexBufferView;
u32 planeIndexCount;
D3D12_VERTEX_BUFFER_VIEW cubeVertexBufferView;
D3D12_INDEX_BUFFER_VIEW cubeIndexBufferView;
u32 cubeIndexCount;

// D3D12 Descriptors
ID3D12DescriptorHeap* rtvDescriptorHeap;
u64 rtvDescriptorSize;
ID3D12DescriptorHeap* dsDescriptorHeap;

//pipeline info
const u32 dwSampleRate = 1;
ID3D12RootSignature* rootSignature; // root signature defines data shaders will access
ID3D12PipelineState* pipelineStateObject; // pso containing a pipeline state

//Model Upload Syncronization
ID3D12Fence* streamingFence;
u64 currStreamingFenceValue;
u64 streamingFenceValue;
HANDLE fenceEvent;

//D3D12 Debug
#if MAIN_DEBUG
ID3D12Debug *debugInterface;
ID3D12InfoQueue *pIQueue; 
#endif

//Constant Buffers
vertexShaderCB vertexConstantBuffer;
pixelShaderCB pixelConstantBuffer;


//Camera
Vec3f startingPos;
f32 rotHor;
f32 rotVert;


inline
void InitMat3f( Mat3f *a_pMat )
{
	a_pMat->m[0][0] = 1; a_pMat->m[0][1] = 0; a_pMat->m[0][2] = 0;
	a_pMat->m[1][0] = 0; a_pMat->m[1][1] = 1; a_pMat->m[1][2] = 0;
	a_pMat->m[2][0] = 0; a_pMat->m[2][1] = 0; a_pMat->m[2][2] = 1;
}

inline
void InitMat4f( Mat4f *a_pMat )
{
	a_pMat->m[0][0] = 1; a_pMat->m[0][1] = 0; a_pMat->m[0][2] = 0; a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0; a_pMat->m[1][1] = 1; a_pMat->m[1][2] = 0; a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0; a_pMat->m[2][1] = 0; a_pMat->m[2][2] = 1; a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = 0; a_pMat->m[3][1] = 0; a_pMat->m[3][2] = 0; a_pMat->m[3][3] = 1;
}

inline
void InitTransMat4f( Mat4f *a_pMat, f32 x, f32 y, f32 z )
{
	a_pMat->m[0][0] = 1; a_pMat->m[0][1] = 0; a_pMat->m[0][2] = 0; a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0; a_pMat->m[1][1] = 1; a_pMat->m[1][2] = 0; a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0; a_pMat->m[2][1] = 0; a_pMat->m[2][2] = 1; a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = x; a_pMat->m[3][1] = y; a_pMat->m[3][2] = z; a_pMat->m[3][3] = 1;
}

inline
void InitTransMat4f( Mat4f *a_pMat, Vec3f *a_pTrans )
{
	a_pMat->m[0][0] = 1;           a_pMat->m[0][1] = 0;           a_pMat->m[0][2] = 0;           a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0;           a_pMat->m[1][1] = 1;           a_pMat->m[1][2] = 0;           a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0;           a_pMat->m[2][1] = 0;           a_pMat->m[2][2] = 1;           a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = a_pTrans->x; a_pMat->m[3][1] = a_pTrans->y; a_pMat->m[3][2] = a_pTrans->z; a_pMat->m[3][3] = 1;
}

/*
inline
void InitRotXMat4f( Mat4f *a_pMat, f32 angle )
{
	a_pMat->m[0][0] = 1; a_pMat->m[0][1] = 0;                        a_pMat->m[0][2] = 0;                       a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0; a_pMat->m[1][1] = cosf(angle*PI_F/180.0f);  a_pMat->m[1][2] = sinf(angle*PI_F/180.0f); a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0; a_pMat->m[2][1] = -sinf(angle*PI_F/180.0f); a_pMat->m[2][2] = cosf(angle*PI_F/180.0f); a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = 0; a_pMat->m[3][1] = 0;                        a_pMat->m[3][2] = 0;                       a_pMat->m[3][3] = 1;
}

inline
void InitRotYMat4f( Mat4f *a_pMat, f32 angle )
{
	a_pMat->m[0][0] = cosf(angle*PI_F/180.0f);  a_pMat->m[0][1] = 0; a_pMat->m[0][2] = -sinf(angle*PI_F/180.0f); a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0;                        a_pMat->m[1][1] = 1; a_pMat->m[1][2] = 0;                        a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = sinf(angle*PI_F/180.0f);  a_pMat->m[2][1] = 0; a_pMat->m[2][2] = cosf(angle*PI_F/180.0f);  a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = 0;                        a_pMat->m[3][1] = 0; a_pMat->m[3][2] = 0;                        a_pMat->m[3][3] = 1;
}

inline
void InitRotZMat4f( Mat4f *a_pMat, f32 angle )
{
	a_pMat->m[0][0] = cosf(angle*PI_F/180.0f);  a_pMat->m[0][1] = sinf(angle*PI_F/180.0f); a_pMat->m[0][2] = 0; a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = -sinf(angle*PI_F/180.0f); a_pMat->m[1][1] = cosf(angle*PI_F/180.0f); a_pMat->m[1][2] = 0; a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0;                        a_pMat->m[2][1] = 0; 					   a_pMat->m[2][2] = 1; a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = 0;                        a_pMat->m[3][1] = 0;                       a_pMat->m[3][2] = 0; a_pMat->m[3][3] = 1;
}
*/

inline
void InitRotArbAxisMat4f( Mat4f *a_pMat, Vec3f *a_pAxis, f32 angle )
{
	f32 c = cosf(angle*PI_F/180.0f);
	f32 mC = 1.0f-c;
	f32 s = sinf(angle*PI_F/180.0f);
	a_pMat->m[0][0] = c                          + (a_pAxis->x*a_pAxis->x*mC); a_pMat->m[0][1] = (a_pAxis->y*a_pAxis->x*mC) + (a_pAxis->z*s);             a_pMat->m[0][2] = (a_pAxis->z*a_pAxis->x*mC) - (a_pAxis->y*s);             a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = (a_pAxis->x*a_pAxis->y*mC) - (a_pAxis->z*s);             a_pMat->m[1][1] = c                          + (a_pAxis->y*a_pAxis->y*mC); a_pMat->m[1][2] = (a_pAxis->z*a_pAxis->y*mC) + (a_pAxis->x*s);             a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = (a_pAxis->x*a_pAxis->z*mC) + (a_pAxis->y*s);             a_pMat->m[2][1] = (a_pAxis->y*a_pAxis->z*mC) - (a_pAxis->x*s);             a_pMat->m[2][2] = c                          + (a_pAxis->z*a_pAxis->z*mC); a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = 0;                                                       a_pMat->m[3][1] = 0;                                                       a_pMat->m[3][2] = 0;                                                       a_pMat->m[3][3] = 1;
}


//Following are DirectX Matrices
inline
void InitPerspectiveProjectionMat4fDirectXRH( Mat4f *a_pMat, u64 width, u64 height, f32 a_hFOV, f32 a_vFOV, f32 nearPlane, f32 farPlane )
{
	f32 thFOV = tanf(a_hFOV*PI_F/360);
	f32 tvFOV = tanf(a_vFOV*PI_F/360);
	f32 nMinF = farPlane/(nearPlane-farPlane);
  	f32 aspect = height / (f32)width;
	a_pMat->m[0][0] = aspect/(thFOV); a_pMat->m[0][1] = 0;            a_pMat->m[0][2] = 0;               a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0;              a_pMat->m[1][1] = 1.0f/(tvFOV); a_pMat->m[1][2] = 0;               a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0;              a_pMat->m[2][1] = 0;            a_pMat->m[2][2] = nMinF;           a_pMat->m[2][3] = -1.0f;
	a_pMat->m[3][0] = 0;              a_pMat->m[3][1] = 0;            a_pMat->m[3][2] = nearPlane*nMinF; a_pMat->m[3][3] = 0;
}

inline
void InitPerspectiveProjectionMat4fDirectXLH( Mat4f *a_pMat, u64 width, u64 height, f32 a_hFOV, f32 a_vFOV, f32 nearPlane, f32 farPlane )
{
	f32 thFOV = tanf(a_hFOV*PI_F/360);
	f32 tvFOV = tanf(a_vFOV*PI_F/360);
	f32 nMinF = farPlane/(nearPlane-farPlane);
  	f32 aspect = height / (f32)width;
	a_pMat->m[0][0] = aspect/(thFOV); a_pMat->m[0][1] = 0;            a_pMat->m[0][2] = 0;               a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0;              a_pMat->m[1][1] = 1.0f/(tvFOV); a_pMat->m[1][2] = 0;               a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 0;              a_pMat->m[2][1] = 0;            a_pMat->m[2][2] = -nMinF;          a_pMat->m[2][3] = 1.0f;
	a_pMat->m[3][0] = 0;              a_pMat->m[3][1] = 0;            a_pMat->m[3][2] = nearPlane*nMinF; a_pMat->m[3][3] = 0;
}

inline
void InitPerspectiveProjectionMat4fOculusDirectXLH( Mat4f *a_pMat, ovrFovPort tanHalfFov, f32 nearPlane, f32 farPlane )
{
    f32 projXScale = 2.0f / ( tanHalfFov.LeftTan + tanHalfFov.RightTan );
    f32 projXOffset = ( tanHalfFov.LeftTan - tanHalfFov.RightTan ) * projXScale * 0.5f;
    f32 projYScale = 2.0f / ( tanHalfFov.UpTan + tanHalfFov.DownTan );
    f32 projYOffset = ( tanHalfFov.UpTan - tanHalfFov.DownTan ) * projYScale * 0.5f;
	f32 nMinF = farPlane/(nearPlane-farPlane);
	a_pMat->m[0][0] = projXScale;  a_pMat->m[0][1] = 0;            a_pMat->m[0][2] = 0;               a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0;           a_pMat->m[1][1] = projYScale;   a_pMat->m[1][2] = 0;               a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = projXOffset; a_pMat->m[2][1] = -projYOffset; a_pMat->m[2][2] = -nMinF;          a_pMat->m[2][3] = 1.0f;
	a_pMat->m[3][0] = 0;           a_pMat->m[3][1] = 0;            a_pMat->m[3][2] = nearPlane*nMinF; a_pMat->m[3][3] = 0;
}


inline
void InitPerspectiveProjectionMat4fOculusDirectXRH( Mat4f *a_pMat, ovrFovPort tanHalfFov, f32 nearPlane, f32 farPlane )
{
    f32 projXScale = 2.0f / ( tanHalfFov.LeftTan + tanHalfFov.RightTan );
    f32 projXOffset = ( tanHalfFov.LeftTan - tanHalfFov.RightTan ) * projXScale * 0.5f;
    f32 projYScale = 2.0f / ( tanHalfFov.UpTan + tanHalfFov.DownTan );
    f32 projYOffset = ( tanHalfFov.UpTan - tanHalfFov.DownTan ) * projYScale * 0.5f;
	f32 nMinF = farPlane/(nearPlane-farPlane);
	a_pMat->m[0][0] = projXScale;   a_pMat->m[0][1] = 0;           a_pMat->m[0][2] = 0;               a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 0;            a_pMat->m[1][1] = projYScale;  a_pMat->m[1][2] = 0;               a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = -projXOffset; a_pMat->m[2][1] = projYOffset; a_pMat->m[2][2] = nMinF;           a_pMat->m[2][3] = -1.0f;
	a_pMat->m[3][0] = 0;            a_pMat->m[3][1] = 0;           a_pMat->m[3][2] = nearPlane*nMinF; a_pMat->m[3][3] = 0;
}

inline
f32 DeterminantUpper3x3Mat4f( Mat4f *a_pMat )
{
	return (a_pMat->m[0][0] * ((a_pMat->m[1][1]*a_pMat->m[2][2]) - (a_pMat->m[1][2]*a_pMat->m[2][1]))) + 
		   (a_pMat->m[0][1] * ((a_pMat->m[2][0]*a_pMat->m[1][2]) - (a_pMat->m[1][0]*a_pMat->m[2][2]))) + 
		   (a_pMat->m[0][2] * ((a_pMat->m[1][0]*a_pMat->m[2][1]) - (a_pMat->m[2][0]*a_pMat->m[1][1])));
}

inline
void InverseUpper3x3Mat4f( Mat4f *__restrict a_pMat, Mat4f *__restrict out )
{
	f32 fDet = DeterminantUpper3x3Mat4f( a_pMat );
#if MAIN_DEBUG
	assert( fDet != 0.f );
#endif
	f32 fInvDet = 1.0f / fDet;
	out->m[0][0] = fInvDet * ((a_pMat->m[1][1]*a_pMat->m[2][2]) - (a_pMat->m[1][2]*a_pMat->m[2][1]));
	out->m[0][1] = fInvDet * ((a_pMat->m[0][2]*a_pMat->m[2][1]) - (a_pMat->m[0][1]*a_pMat->m[2][2]));
	out->m[0][2] = fInvDet * ((a_pMat->m[0][1]*a_pMat->m[1][2]) - (a_pMat->m[0][2]*a_pMat->m[1][1]));
	out->m[0][3] = 0.0f;

	out->m[1][0] = fInvDet * ((a_pMat->m[2][0]*a_pMat->m[1][2]) - (a_pMat->m[2][2]*a_pMat->m[1][0]));
	out->m[1][1] = fInvDet * ((a_pMat->m[0][0]*a_pMat->m[2][2]) - (a_pMat->m[0][2]*a_pMat->m[2][0])); 
	out->m[1][2] = fInvDet * ((a_pMat->m[0][2]*a_pMat->m[1][0]) - (a_pMat->m[1][2]*a_pMat->m[0][0]));
	out->m[1][3] = 0.0f;

	out->m[2][0] = fInvDet * ((a_pMat->m[1][0]*a_pMat->m[2][1]) - (a_pMat->m[1][1]*a_pMat->m[2][0]));
	out->m[2][1] = fInvDet * ((a_pMat->m[0][1]*a_pMat->m[2][0]) - (a_pMat->m[0][0]*a_pMat->m[2][1]));
	out->m[2][2] = fInvDet * ((a_pMat->m[0][0]*a_pMat->m[1][1]) - (a_pMat->m[1][0]*a_pMat->m[0][1]));
	out->m[2][3] = 0.0f;

	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

inline
void InverseTransposeUpper3x3Mat4f( Mat4f *__restrict a_pMat, Mat4f *__restrict out )
{
	f32 fDet = DeterminantUpper3x3Mat4f( a_pMat );
#if MAIN_DEBUG
	assert( fDet != 0.f );
#endif
	f32 fInvDet = 1.0f / fDet;
	out->m[0][0] = fInvDet * ((a_pMat->m[1][1]*a_pMat->m[2][2]) - (a_pMat->m[1][2]*a_pMat->m[2][1]));
	out->m[0][1] = fInvDet * ((a_pMat->m[2][0]*a_pMat->m[1][2]) - (a_pMat->m[2][2]*a_pMat->m[1][0]));
	out->m[0][2] = fInvDet * ((a_pMat->m[1][0]*a_pMat->m[2][1]) - (a_pMat->m[1][1]*a_pMat->m[2][0]));
	out->m[0][3] = 0.0f;

	out->m[1][0] = fInvDet * ((a_pMat->m[0][2]*a_pMat->m[2][1]) - (a_pMat->m[0][1]*a_pMat->m[2][2]));
	out->m[1][1] = fInvDet * ((a_pMat->m[0][0]*a_pMat->m[2][2]) - (a_pMat->m[0][2]*a_pMat->m[2][0])); 
	out->m[1][2] = fInvDet * ((a_pMat->m[0][1]*a_pMat->m[2][0]) - (a_pMat->m[0][0]*a_pMat->m[2][1]));
	out->m[1][3] = 0.0f;

	out->m[2][0] = fInvDet * ((a_pMat->m[0][1]*a_pMat->m[1][2]) - (a_pMat->m[0][2]*a_pMat->m[1][1]));
	out->m[2][1] = fInvDet * ((a_pMat->m[0][2]*a_pMat->m[1][0]) - (a_pMat->m[1][2]*a_pMat->m[0][0]));
	out->m[2][2] = fInvDet * ((a_pMat->m[0][0]*a_pMat->m[1][1]) - (a_pMat->m[1][0]*a_pMat->m[0][1]));
	out->m[2][3] = 0.0f;

	out->m[3][0] = 0.0f;
	out->m[3][1] = 0.0f;
	out->m[3][2] = 0.0f;
	out->m[3][3] = 1.0f;
}

inline
void InverseTransposeUpper3x3Mat4f( Mat4f *__restrict a_pMat, Mat3x4f *__restrict out )
{
	f32 fDet = DeterminantUpper3x3Mat4f( a_pMat );
#if MAIN_DEBUG
	assert( fDet != 0.f );
#endif
	f32 fInvDet = 1.0f / fDet;
	out->m[0][0] = fInvDet * ((a_pMat->m[1][1]*a_pMat->m[2][2]) - (a_pMat->m[1][2]*a_pMat->m[2][1]));
	out->m[0][1] = fInvDet * ((a_pMat->m[2][0]*a_pMat->m[1][2]) - (a_pMat->m[2][2]*a_pMat->m[1][0]));
	out->m[0][2] = fInvDet * ((a_pMat->m[1][0]*a_pMat->m[2][1]) - (a_pMat->m[1][1]*a_pMat->m[2][0]));
	out->m[0][3] = 0.0f;

	out->m[1][0] = fInvDet * ((a_pMat->m[0][2]*a_pMat->m[2][1]) - (a_pMat->m[0][1]*a_pMat->m[2][2]));
	out->m[1][1] = fInvDet * ((a_pMat->m[0][0]*a_pMat->m[2][2]) - (a_pMat->m[0][2]*a_pMat->m[2][0])); 
	out->m[1][2] = fInvDet * ((a_pMat->m[0][1]*a_pMat->m[2][0]) - (a_pMat->m[0][0]*a_pMat->m[2][1]));
	out->m[1][3] = 0.0f;

	out->m[2][0] = fInvDet * ((a_pMat->m[0][1]*a_pMat->m[1][2]) - (a_pMat->m[0][2]*a_pMat->m[1][1]));
	out->m[2][1] = fInvDet * ((a_pMat->m[0][2]*a_pMat->m[1][0]) - (a_pMat->m[1][2]*a_pMat->m[0][0]));
	out->m[2][2] = fInvDet * ((a_pMat->m[0][0]*a_pMat->m[1][1]) - (a_pMat->m[1][0]*a_pMat->m[0][1]));
	out->m[2][3] = 0.0f;
}


inline
void Mat4fMult( Mat4f *__restrict a, Mat4f *__restrict b, Mat4f *__restrict out)
{
	out->m[0][0] = a->m[0][0]*b->m[0][0] + a->m[0][1]*b->m[1][0] + a->m[0][2]*b->m[2][0] + a->m[0][3]*b->m[3][0];
	out->m[0][1] = a->m[0][0]*b->m[0][1] + a->m[0][1]*b->m[1][1] + a->m[0][2]*b->m[2][1] + a->m[0][3]*b->m[3][1];
	out->m[0][2] = a->m[0][0]*b->m[0][2] + a->m[0][1]*b->m[1][2] + a->m[0][2]*b->m[2][2] + a->m[0][3]*b->m[3][2];
	out->m[0][3] = a->m[0][0]*b->m[0][3] + a->m[0][1]*b->m[1][3] + a->m[0][2]*b->m[2][3] + a->m[0][3]*b->m[3][3];

	out->m[1][0] = a->m[1][0]*b->m[0][0] + a->m[1][1]*b->m[1][0] + a->m[1][2]*b->m[2][0] + a->m[1][3]*b->m[3][0];
	out->m[1][1] = a->m[1][0]*b->m[0][1] + a->m[1][1]*b->m[1][1] + a->m[1][2]*b->m[2][1] + a->m[1][3]*b->m[3][1];
	out->m[1][2] = a->m[1][0]*b->m[0][2] + a->m[1][1]*b->m[1][2] + a->m[1][2]*b->m[2][2] + a->m[1][3]*b->m[3][2];
	out->m[1][3] = a->m[1][0]*b->m[0][3] + a->m[1][1]*b->m[1][3] + a->m[1][2]*b->m[2][3] + a->m[1][3]*b->m[3][3];

	out->m[2][0] = a->m[2][0]*b->m[0][0] + a->m[2][1]*b->m[1][0] + a->m[2][2]*b->m[2][0] + a->m[2][3]*b->m[3][0];
	out->m[2][1] = a->m[2][0]*b->m[0][1] + a->m[2][1]*b->m[1][1] + a->m[2][2]*b->m[2][1] + a->m[2][3]*b->m[3][1];
	out->m[2][2] = a->m[2][0]*b->m[0][2] + a->m[2][1]*b->m[1][2] + a->m[2][2]*b->m[2][2] + a->m[2][3]*b->m[3][2];
	out->m[2][3] = a->m[2][0]*b->m[0][3] + a->m[2][1]*b->m[1][3] + a->m[2][2]*b->m[2][3] + a->m[2][3]*b->m[3][3];

	out->m[3][0] = a->m[3][0]*b->m[0][0] + a->m[3][1]*b->m[1][0] + a->m[3][2]*b->m[2][0] + a->m[3][3]*b->m[3][0];
	out->m[3][1] = a->m[3][0]*b->m[0][1] + a->m[3][1]*b->m[1][1] + a->m[3][2]*b->m[2][1] + a->m[3][3]*b->m[3][1];
	out->m[3][2] = a->m[3][0]*b->m[0][2] + a->m[3][1]*b->m[1][2] + a->m[3][2]*b->m[2][2] + a->m[3][3]*b->m[3][2];
	out->m[3][3] = a->m[3][0]*b->m[0][3] + a->m[3][1]*b->m[1][3] + a->m[3][2]*b->m[2][3] + a->m[3][3]*b->m[3][3];
}

inline
void Vec3fAdd( Vec3f *a, Vec3f *b, Vec3f *out )
{
	out->x = a->x + b->x;
	out->y = a->y + b->y;
	out->z = a->z + b->z;
}

inline
void Vec3fSub( Vec3f *a, Vec3f *b, Vec3f *out )
{
	out->x = a->x - b->x;
	out->y = a->y - b->y;
	out->z = a->z - b->z;
}

inline
void Vec3fMult( Vec3f *a, Vec3f *b, Vec3f *out )
{
	out->x = a->x * b->x;
	out->y = a->y * b->y;
	out->z = a->z * b->z;
}

inline
void Vec3fCross( Vec3f *a, Vec3f *b, Vec3f *out )
{
	out->x = (a->y * b->z) - (a->z * b->y);
	out->y = (a->z * b->x) - (a->x * b->z);
	out->z = (a->x * b->y) - (a->y * b->x);
}

inline
void Vec3fScale( Vec3f *a, f32 scale, Vec3f *out )
{
	out->x = a->x * scale;
	out->y = a->y * scale;
	out->z = a->z * scale;
}

inline
f32 Vec3fDot( Vec3f *a, Vec3f *b )
{
	return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

inline
void Vec3fNormalize( Vec3f *a, Vec3f *out )
{

	f32 mag = sqrtf((a->x*a->x) + (a->y*a->y) + (a->z*a->z));
	if(mag == 0)
	{
		out->x = 0;
		out->y = 0;
		out->z = 0;
	}
	else
	{
		out->x = a->x/mag;
		out->y = a->y/mag;
		out->z = a->z/mag;
	}
}


inline
void Vec3fRotByUnitQuat(Vec3f *v, Quatf *__restrict q, Vec3f *out)
{
    f32 fVecScalar = (2.0f*q->w*q->w)-1;
    f32 fQuatVecScalar = 2.0f* Vec3fDot(v,&q->v);

    Vec3f vScaledQuatVec;
    Vec3f vScaledVec;
    Vec3fScale(&q->v,fQuatVecScalar,&vScaledQuatVec);
    Vec3fScale(v,fVecScalar,&vScaledVec);

    Vec3f vQuatCrossVec;
    Vec3fCross(&q->v, v, &vQuatCrossVec);

    Vec3fScale(&vQuatCrossVec,2.0f*q->w,&vQuatCrossVec);

    Vec3fAdd(&vScaledQuatVec,&vScaledVec,out);
    Vec3fAdd(out,&vQuatCrossVec,out);
}

/*
inline
void Vec3fRotByUnitQuat(Vec3f *v, Quatf *__restrict q, Vec3f *out)
{
	Vec3f vDoubleRot;
	vDoubleRot.x = q->x + q->x;
	vDoubleRot.y = q->y + q->y;
	vDoubleRot.z = q->z + q->z;

	Vec3f vScaledWRot;
	vScaledWRot.x = q->w * vDoubleRot.x;
	vScaledWRot.y = q->w * vDoubleRot.y;
	vScaledWRot.z = q->w * vDoubleRot.z;

	Vec3f vScaledXRot;
	vScaledXRot.x = q->x * vDoubleRot.x;
	vScaledXRot.y = q->x * vDoubleRot.y;
	vScaledXRot.z = q->x * vDoubleRot.z;

	f32 fScaledYRot0 = q->y * vDoubleRot.y;
	f32 fScaledYRot1 = q->y * vDoubleRot.z;

	f32 fScaledZRot0 = q->z * vDoubleRot.z;

	out->x = ((v->x * ((1.f - fScaledYRot0) - fScaledZRot0)) + (v->y * (vScaledXRot.y - vScaledWRot.z))) + (v->z * (vScaledXRot.z + vScaledWRot.y));
	out->y = ((v->x * (vScaledXRot.y + vScaledWRot.z)) + (v->y * ((1.f - vScaledXRot.x) - fScaledZRot0))) + (v->z * (fScaledYRot1 - vScaledWRot.x));
	out->z = ((v->x * (vScaledXRot.z - vScaledWRot.y)) + (v->y * (fScaledYRot1 + vScaledWRot.x))) + (v->z * ((1.f - vScaledXRot.x) - fScaledYRot0));
}
*/


inline
void InitUnitQuatf( Quatf *q, f32 angle, Vec3f *axis )
{
	f32 s = sinf(angle*PI_F/360.0f);
	q->w = cosf(angle*PI_F/360.0f);
	q->x = axis->x * s;
	q->y = axis->y * s;
	q->z = axis->z * s;
}

inline
void QuatfMult( Quatf *__restrict a, Quatf *__restrict b, Quatf *__restrict out )
{
	out->w = (a->w * b->w) - (a->x* b->x) - (a->y* b->y) - (a->z* b->z);
	out->x = (a->w * b->x) + (a->x* b->w) + (a->y* b->z) - (a->z* b->y);
	out->y = (a->w * b->y) + (a->y* b->w) + (a->z* b->x) - (a->x* b->z);
	out->z = (a->w * b->z) + (a->z* b->w) + (a->x* b->y) - (a->y* b->x);
}


//todo simplify to reduce floating point error
inline
void InitViewMat4ByQuatf( Mat4f *a_pMat, Quatf *a_qRot, Vec3f *a_pPos )
{
	a_pMat->m[0][0] = 1.0f - 2.0f*(a_qRot->y*a_qRot->y + a_qRot->z*a_qRot->z);                            a_pMat->m[0][1] = 2.0f*(a_qRot->x*a_qRot->y - a_qRot->w*a_qRot->z);                                   a_pMat->m[0][2] = 2.0f*(a_qRot->x*a_qRot->z + a_qRot->w*a_qRot->y);        		                      a_pMat->m[0][3] = 0;
	a_pMat->m[1][0] = 2.0f*(a_qRot->x*a_qRot->y + a_qRot->w*a_qRot->z);                                   a_pMat->m[1][1] = 1.0f - 2.0f*(a_qRot->x*a_qRot->x + a_qRot->z*a_qRot->z);                            a_pMat->m[1][2] = 2.0f*(a_qRot->y*a_qRot->z - a_qRot->w*a_qRot->x);        		                      a_pMat->m[1][3] = 0;
	a_pMat->m[2][0] = 2.0f*(a_qRot->x*a_qRot->z - a_qRot->w*a_qRot->y);                                   a_pMat->m[2][1] = 2.0f*(a_qRot->y*a_qRot->z + a_qRot->w*a_qRot->x);                                   a_pMat->m[2][2] = 1.0f - 2.0f*(a_qRot->x*a_qRot->x + a_qRot->y*a_qRot->y); 		                      a_pMat->m[2][3] = 0;
	a_pMat->m[3][0] = -a_pPos->x*a_pMat->m[0][0] - a_pPos->y*a_pMat->m[1][0] - a_pPos->z*a_pMat->m[2][0]; a_pMat->m[3][1] = -a_pPos->x*a_pMat->m[0][1] - a_pPos->y*a_pMat->m[1][1] - a_pPos->z*a_pMat->m[2][1]; a_pMat->m[3][2] = -a_pPos->x*a_pMat->m[0][2] - a_pPos->y*a_pMat->m[1][2] - a_pPos->z*a_pMat->m[2][2]; a_pMat->m[3][3] = 1;
}

#if MAIN_DEBUG
void PrintMat4f( Mat4f *a_pMat )
{
	for( u32 dwIdx = 0; dwIdx < 4; ++dwIdx )
	{
		for( u32 dwJdx = 0; dwJdx < 4; ++dwJdx )
		{
			printf("%f ", a_pMat->m[dwIdx][dwJdx] );
		}
		printf("\n");
	}
}
#endif

f32 clamp(f32 d, f32 min, f32 max) {
  const f32 t = d < min ? min : d;
  return t > max ? max : t;
}

u32 max( u32 a, u32 b )
{
	return a > b ? a : b;
}


int logError(const char* msg)
{
#if MAIN_DEBUG
	MessageBoxA(NULL, msg, "Error", MB_OK | MB_ICONERROR);
#endif
    return -1;
}

void CloseProgram()
{
	Running = 0;
}

void Pause()
{
	if( !isPaused )
	{
		isPaused = 1;
	}
}

void UnPause()
{
	if( isPaused )
	{
		isPaused = 0;
	}
}

void TogglePause()
{
    isPaused = isPaused ^ 1;
}

inline
void InitStartingGameState()
{
	Running = 1;
    isPaused = 0;
}

inline
void InitHeadsetGraphicsState()
{
	pixelConstantBuffer.vLightColor = {0.83137f,0.62745f,0.09020f,1.0f};
	pixelConstantBuffer.vInvLightDir = {0.57735026919f,0.57735026919f,0.57735026919f};
}

inline
void InitStartingCamera()
{
	startingPos = { 0, 0, 0 };
	rotHor = 0;
	rotVert = 0;
}

//verify the following are right!
inline
bool SignalStreamingFence()
{
	++streamingFenceValue;
    if( FAILED( commandQueue->Signal( streamingFence, streamingFenceValue ) ) )
    {
    	CloseProgram();
    	logError( "Error signalling fence!\n" ); 
    	return false;
    }
    return true;
}

inline
bool WaitStreamingFence()
{
	if( streamingFence->GetCompletedValue() < streamingFenceValue )
    {
    	if( FAILED( streamingFence->SetEventOnCompletion( streamingFenceValue, fenceEvent ) ) )
		{
			CloseProgram();
			logError( "Failed to set fence event!\n" );
			return false;
		}
		WaitForSingleObject( fenceEvent, INFINITE );
    }
    return true;
}

//append a signal to the end of the command queue and wait for it
inline
bool FlushStreamingCommandQueue()
{
   	if( !SignalStreamingFence() )
   	{
   		return false;
   	}
   	if( !WaitStreamingFence() )
   	{
		return false;
   	}
   	return true;
}


u8 InitOculusHeadset()
{
	oculusFrameIndex = 0;
	if( ovr_Create( &oculusSession, &oculusGLuid ) < 0 )
	{
#if MAIN_DEBUG
    	//TODO change to a retry create head set
    	printf("ovr_Create failed\n");
#endif		
    	return 1;
	}

	//Get Head Mounted Display Description
	oculusHMDDesc = ovr_GetHmdDesc( oculusSession );
#if MAIN_DEBUG
	printf( "Headset: %s\n",&oculusHMDDesc.ProductName[0] );
	printf( "Made By: %s\n",&oculusHMDDesc.Manufacturer[0] );
	printf( "Firmware Ver: %d.%d\n",oculusHMDDesc.FirmwareMajor,oculusHMDDesc.FirmwareMinor );
	printf( "HMD Resolution: %d x %d\n",oculusHMDDesc.Resolution.w, oculusHMDDesc.Resolution.h );
	for( u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye )
	{
		printf("Eye %d stats:\n", dwEye);
		f32 fUpFOV    = (360.f / PI_F) * atanf( oculusHMDDesc.DefaultEyeFov[dwEye].UpTan );
		f32 fDownFOV  = (360.f / PI_F) * atanf( oculusHMDDesc.DefaultEyeFov[dwEye].DownTan );
		f32 fRightFOV = (360.f / PI_F) * atanf( oculusHMDDesc.DefaultEyeFov[dwEye].RightTan );
		f32 fLeftFOV  = (360.f / PI_F) * atanf( oculusHMDDesc.DefaultEyeFov[dwEye].LeftTan );
		f32 fMaxUpFOV    = (360.f / PI_F) * atanf( oculusHMDDesc.MaxEyeFov[dwEye].UpTan );
		f32 fMaxDownFOV  = (360.f / PI_F) * atanf( oculusHMDDesc.MaxEyeFov[dwEye].DownTan );
		f32 fMaxRightFOV = (360.f / PI_F) * atanf( oculusHMDDesc.MaxEyeFov[dwEye].RightTan );
		f32 fMaxLeftFOV  = (360.f / PI_F) * atanf( oculusHMDDesc.MaxEyeFov[dwEye].LeftTan );
		printf("    Default FOV: Up %f Down %f Left %f Right %f\n", fUpFOV, fDownFOV, fRightFOV, fLeftFOV );
		printf("    Max FOV: Up %f Down %f Left %f Right %f\n", fMaxUpFOV, fMaxDownFOV, fMaxRightFOV, fMaxLeftFOV );
	}
#endif
	return 0;
}

#if MAIN_DEBUG
inline
bool EnableDebugLayer()
{
    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    if ( FAILED( D3D12GetDebugInterface( IID_PPV_ARGS( &debugInterface ) ) ) )
    {
    	logError( "Error creating DirectX12 Debug layer\n" );  
        return false;
    }
    debugInterface->EnableDebugLayer();
    return true;
}
#endif

inline
ID3D12CommandQueue *InitDirectCommandQueue( ID3D12Device* dxd3Device )
{
	ID3D12CommandQueue *cq;
	D3D12_COMMAND_QUEUE_DESC cqDesc;
    cqDesc.Type =     D3D12_COMMAND_LIST_TYPE_DIRECT;
    cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cqDesc.Flags =    D3D12_COMMAND_QUEUE_FLAG_NONE;
    cqDesc.NodeMask = 0;

    if( FAILED( dxd3Device->CreateCommandQueue( &cqDesc, IID_PPV_ARGS( &cq ) ) ) )
	{
		return NULL;
	}
	return cq;
}

inline
ID3D12DescriptorHeap *InitRenderTargetDescriptorHeap( ID3D12Device* dxd3Device, u32 dwAmt )
{
	ID3D12DescriptorHeap *rtvHeap;
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = dwAmt; 
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	if( FAILED( dxd3Device->CreateDescriptorHeap( &rtvHeapDesc, IID_PPV_ARGS( &rtvHeap ) ) ) )
	{
		return NULL;
	}
	return rtvHeap;
}


inline
ID3D12DescriptorHeap *InitDepthStencilDescriptorHeap( ID3D12Device* dxd3Device, u32 dwAmt )
{
	ID3D12DescriptorHeap *dsvHeap;
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = dwAmt;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	if( FAILED( dxd3Device->CreateDescriptorHeap( &dsvHeapDesc, IID_PPV_ARGS( &dsvHeap ) ) ) )
	{
		return NULL;
	}
	return dsvHeap;
}

inline
bool CreateDepthStencilBuffer()
{
	/*
	frame time->
	|--- prev ---|--- curr ---|--- next ---|

	we only need 1 depth buffer currently, since the cpu is not mutating it while it is actively rendering (unlike uniforms). 
	and it is reset within the command lists and only actively used within the current frame rendering. (it is synchronous within the command list, this is not always the case, but here it is) (unlike swap chain buffers which cannot be mutated until after the screen is done with them)

	only 1 frame will be using it as a time, since one frame doesn't start rendering until the last one is finished executing its command lists (maybe in future programs, this won't be true)
	
	hence we only need 1 depth buffer here.

	"Since we are only using a single command queue for rendering, then all of the rendering commands are executed serially in the queue so the depth buffer will not be accessed by multiple queues at the same time. The reason why we need multiple color buffers is because we can’t write the next frame until the Windows Driver Display Model (WDDM) is finished presenting the frame on the screen. Stalling the CPU thread until the frame is finished being presented is not efficient so to avoid the stall, we simply render to a different (front) buffer. Since the depth buffer is not used by WDDM, we don’t need to create multiple depth buffers!"

	Note: You cannot use this trick if you need GBuffer Depth information!
	
	*/

	D3D12_HEAP_PROPERTIES depthBuffHeapBufferDesc; //describes heap type
	depthBuffHeapBufferDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthBuffHeapBufferDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthBuffHeapBufferDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	depthBuffHeapBufferDesc.CreationNodeMask = 1;
	depthBuffHeapBufferDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC depthBufferDesc; //describes what is placed in heap
  	depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  	depthBufferDesc.Alignment = 0;
  	depthBufferDesc.DepthOrArraySize = 1;
  	depthBufferDesc.MipLevels = 0;
  	depthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
  	depthBufferDesc.SampleDesc.Count = 1;
  	depthBufferDesc.SampleDesc.Quality = 0;
  	depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  	depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // D3D12_DSV_DIMENSION_TEXTURE2DMS for anti aliasing
	depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	u64 dsvDescriptorSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );

	for( u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye )
	{
		ovrSizei oculusIdealSize = ovr_GetFovTextureSize( oculusSession, (ovrEyeType)dwEye, oculusHMDDesc.DefaultEyeFov[dwEye], 1.0f );
		depthBufferDesc.Width = oculusIdealSize.w;
  		depthBufferDesc.Height = oculusIdealSize.h;
  		//TODO HOW TO COMBINE INTO 1 HEAP!
		if( FAILED( device->CreateCommittedResource( &depthBuffHeapBufferDesc, D3D12_HEAP_FLAG_NONE, &depthBufferDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS( &depthStencilBuffers[dwEye] ) ) ) )
		{
			logError( "Failed to allocate depth buffer!\n" );
			CloseProgram();
			return false;
		}
#if !MAIN_DEBUG
		depthStencilBuffers[dwEye]->SetName(L"Eye Depth/Stencil Default Heap");
#endif
		eyeDSVHandle[dwEye] = dsvHandle;
		device->CreateDepthStencilView( depthStencilBuffers[dwEye], &depthStencilViewDesc, dsvHandle );
		dsvHandle.ptr = (u64)dsvHandle.ptr + dsvDescriptorSize;
	}
	return true;
}

ID3D12Heap* pModelDefaultHeap;
ID3D12Heap* pModelUploadHeap; //will be <= size of the default heap

ID3D12Resource* defaultBuffer; //a default committed resource
ID3D12Resource* uploadBuffer; //a tmp upload committed resource

inline
void UploadModels()
{
	//can we combine vertices and indices into 1 array, do 1 upload, then just have separate views into the default heap?
	f32 planeVertices[] =
	{
		// positions              // vertex norms    // vertex colors
		 1000.0f,  -1.0f,  1000.0f, 0.0f,  1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		-1000.0f,  -1.0f,  1000.0f, 0.0f,  1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		 1000.0f,  -1.0f, -1000.0f, 0.0f,  1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		-1000.0f,  -1.0f, -1000.0f, 0.0f,  1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		 1000.0f,  -1.0f,  1000.0f, 0.0f, -1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		-1000.0f,  -1.0f,  1000.0f, 0.0f, -1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		 1000.0f,  -1.0f, -1000.0f, 0.0f, -1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f,
		-1000.0f,  -1.0f, -1000.0f, 0.0f, -1.0f, 0.0f, 0.5882f, 0.2941f, 0.0f, 1.0f
	};

	u32 planeIndices[] = 
	{
		0, 1, 2,
		2, 1, 3,
		4, 6, 5,
		6, 7, 5
	};

    f32 cubeVertices[] = {
    	// positions          // vertex norms     //vertex colors
       	-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,

       	-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,

       	-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

       	 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

       	-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

       	-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       	-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f
    };

    u32 cubeIndicies[] =
    {
    	 0,  1,  2, //back
    	 3,  4,  5,

    	 6,  7,  8,
    	 9, 10, 11,

    	12, 13, 14,
    	15, 16, 17,

    	18, 20, 19,
    	21, 23, 22,

    	24, 25, 26, //bottom
    	27, 28, 29,

    	30, 31, 32, //top
    	33, 34, 35
    };


	planeIndexCount = 12;
	cubeIndexCount = 36;

	const u64 qwHeapSize = sizeof(planeVertices) + sizeof(planeIndices) + sizeof(cubeVertices) + sizeof(cubeIndicies);

	//https://zhangdoa.com/posts/walking-through-the-heap-properties-in-directx-12
	//https://asawicki.info/news_1726_secrets_of_direct3d_12_resource_alignment
	//https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_heap_tier#D3D12_RESOURCE_HEAP_TIER_1
	// we only allow buffers to stay within the first tier heap tier
	D3D12_HEAP_PROPERTIES heapBufferDesc; //describes heap type
	heapBufferDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapBufferDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; //potentially D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE since we transfer from Upload heap
	heapBufferDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapBufferDesc.CreationNodeMask = 0;
	heapBufferDesc.VisibleNodeMask = 0;

	D3D12_HEAP_DESC modelHeapDesc;
	modelHeapDesc.SizeInBytes = ((qwHeapSize/65536)*65536) + 65536;
	modelHeapDesc.Properties = heapBufferDesc;
	modelHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; //64KB heap alignment, SizeInBytes should be a multiple of the heap alignment. is 64KB here 65536 or 64000?
	modelHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS; //D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;

	device->CreateHeap( &modelHeapDesc, IID_PPV_ARGS(&pModelDefaultHeap) );
#if MAIN_DEBUG
	pModelDefaultHeap->SetName( L"Model Buffer Default Resource Heap" );
#endif

	modelHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;

	device->CreateHeap( &modelHeapDesc, IID_PPV_ARGS(&pModelUploadHeap) );
#if MAIN_DEBUG
	pModelUploadHeap->SetName( L"Model Buffer Upload Resource Heap" );
#endif

	D3D12_RESOURCE_DESC resourceBufferDesc; //describes what is placed in heap
  	resourceBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  	resourceBufferDesc.Alignment = 0;
  	resourceBufferDesc.Width = qwHeapSize;
  	resourceBufferDesc.Height = 1;
  	resourceBufferDesc.DepthOrArraySize = 1;
  	resourceBufferDesc.MipLevels = 1;
  	resourceBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  	resourceBufferDesc.SampleDesc.Count = 1;
  	resourceBufferDesc.SampleDesc.Quality = 0;
  	resourceBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  	resourceBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE; //D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE

  	//verify that we are using the advanced model!
	device->CreatePlacedResource( pModelUploadHeap, 0, &resourceBufferDesc,D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer) );
	device->CreatePlacedResource( pModelDefaultHeap, 0, &resourceBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&defaultBuffer) );

    //upload to upload heap
    u8* pUploadBufferData;
    if( FAILED( uploadBuffer->Map( 0, nullptr, (void**) &pUploadBufferData ) ) )
    {
        return;
    }
    memcpy(pUploadBufferData,planeVertices,sizeof(planeVertices));
    memcpy(pUploadBufferData+sizeof(planeVertices),planeIndices,sizeof(planeIndices));
    memcpy(pUploadBufferData+sizeof(planeVertices)+sizeof(planeIndices),cubeVertices,sizeof(cubeVertices));
    memcpy(pUploadBufferData+sizeof(planeVertices)+sizeof(planeIndices)+sizeof(cubeVertices),cubeIndicies,sizeof(cubeIndicies));
    uploadBuffer->Unmap( 0, nullptr );

	commandLists[ovrEye_Count]->CopyResource( defaultBuffer, uploadBuffer );
	//commandLists[ovrEye_Count]->CopyBufferRegion( defaultBuffer, 0, uploadBuffer, 0, sizeof(planeVertices)+sizeof(planeIndices)+sizeof(cubeVertices)+sizeof(cubeIndicies) );

	D3D12_RESOURCE_BARRIER defaultHeapUploadToReadBarrier;
    defaultHeapUploadToReadBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    defaultHeapUploadToReadBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    defaultHeapUploadToReadBarrier.Transition.pResource = defaultBuffer;
   	defaultHeapUploadToReadBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    defaultHeapUploadToReadBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    defaultHeapUploadToReadBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    commandLists[ovrEye_Count]->ResourceBarrier( 1, &defaultHeapUploadToReadBarrier );

    planeVertexBufferView.BufferLocation = defaultBuffer->GetGPUVirtualAddress();
    planeVertexBufferView.StrideInBytes = 3*sizeof(f32) + 3*sizeof(f32) + 4*sizeof(f32); //size of s single vertex
    planeVertexBufferView.SizeInBytes = sizeof(planeVertices);

	planeIndexBufferView.BufferLocation = planeVertexBufferView.BufferLocation + sizeof(planeVertices);
    planeIndexBufferView.SizeInBytes = sizeof(planeIndices);
    planeIndexBufferView.Format = DXGI_FORMAT_R32_UINT; 

    cubeVertexBufferView.BufferLocation = planeIndexBufferView.BufferLocation+sizeof(planeIndices);
    cubeVertexBufferView.StrideInBytes = 3*sizeof(f32) + 3*sizeof(f32) + 4*sizeof(f32); //size of s single vertex
    cubeVertexBufferView.SizeInBytes = sizeof(cubeVertices);

	cubeIndexBufferView.BufferLocation = cubeVertexBufferView.BufferLocation+sizeof(cubeVertices);
    cubeIndexBufferView.SizeInBytes = sizeof(cubeIndicies);
    cubeIndexBufferView.Format = DXGI_FORMAT_R32_UINT; 
}

inline
u8 InitDirectX12()
{
	//init directx 12

	//can this factory be downgraded
	IDXGIFactory4 *dxgiFactory;
	if( FAILED( CreateDXGIFactory1( IID_PPV_ARGS( &dxgiFactory ) ) ) )
	{
		logError( "Error creating DXGI factory\n" );  
		return 1;
	}

	//Find the gpu with the headset attachted to it
	IDXGIAdapter* adapter = nullptr;
    for(u32 dwAdapter = 0; dxgiFactory->EnumAdapters(dwAdapter, &adapter) != DXGI_ERROR_NOT_FOUND; ++dwAdapter )
    {
        DXGI_ADAPTER_DESC adapterDesc;
        adapter->GetDesc(&adapterDesc);
        //reinterpret_cast<LUID*>(&oculusGLuid)
        if (memcmp(&adapterDesc.AdapterLuid, &oculusGLuid, sizeof(LUID)) == 0)
        {
        	break;
        }
        adapter->Release();
    }

	//actually retrieve the device interface to the adapter
	if( FAILED( D3D12CreateDevice( adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( &device ) ) ) )
	{
		logError( "Error could not open directx 12 supporting GPU or find GPU with Headset Attached to it!\n" );  
		return 1;
	}
	adapter->Release();

#if MAIN_DEBUG
	//for getting errors from directx when debugging
	if( SUCCEEDED( device->QueryInterface( IID_PPV_ARGS( &pIQueue ) ) ) )
	{
		pIQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pIQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pIQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        

        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };
 
        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };
 
        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;
 
        if( FAILED( pIQueue->PushStorageFilter( &NewFilter ) ) )
        {
        	logError( "Detected device creation problem!\n" );  
        	return 1;
        }
	}
#endif

	commandQueue = InitDirectCommandQueue( device );
	if( !commandQueue )
	{
		logError( "Failed to create command queue!\n" ); 
		return 1;
	}


	{
		ovrTextureSwapChainDesc eyeSwapchainColorTextureDesc;
		eyeSwapchainColorTextureDesc.Type = ovrTexture_2D;
		eyeSwapchainColorTextureDesc.Format =  OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		eyeSwapchainColorTextureDesc.ArraySize = 1;
		eyeSwapchainColorTextureDesc.MipLevels = 1;
		eyeSwapchainColorTextureDesc.SampleCount = dwSampleRate;
		eyeSwapchainColorTextureDesc.StaticImage = ovrFalse;
		eyeSwapchainColorTextureDesc.MiscFlags = ovrTextureMisc_DX_Typeless | ovrTextureMisc_AutoGenerateMips;
		eyeSwapchainColorTextureDesc.BindFlags = ovrTextureBind_DX_RenderTarget;

		for( u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye )
		{
			ovrSizei oculusIdealSize = ovr_GetFovTextureSize( oculusSession, (ovrEyeType)dwEye, oculusHMDDesc.DefaultEyeFov[dwEye], 1.0f );
			
			//setup viewport for where to write output for image
			oculusEyeRenderViewport[dwEye].Pos.x = 0;
			oculusEyeRenderViewport[dwEye].Pos.y = 0;
			oculusEyeRenderViewport[dwEye].Size = oculusIdealSize;

			EyeViewports[dwEye].TopLeftX = 0;
			EyeViewports[dwEye].TopLeftY = 0;
			EyeViewports[dwEye].Width = (f32)oculusIdealSize.w;
			EyeViewports[dwEye].Height = (f32)oculusIdealSize.h;
			EyeViewports[dwEye].MinDepth = 0.0f;
			EyeViewports[dwEye].MaxDepth = 1.0f;

    		EyeScissorRects[dwEye].left = 0;
    		EyeScissorRects[dwEye].top = 0;
    		EyeScissorRects[dwEye].right = oculusIdealSize.w;
    		EyeScissorRects[dwEye].bottom = oculusIdealSize.h;

			//TODO create eye swap chains (is it possible to create both at once by upping thr array size number?)
			eyeSwapchainColorTextureDesc.Width = oculusIdealSize.w;
			eyeSwapchainColorTextureDesc.Height = oculusIdealSize.h;
	
			if( ovr_CreateTextureSwapChainDX( oculusSession, commandQueue, &eyeSwapchainColorTextureDesc, &oculusEyeSwapChains[dwEye] ) < 0 )
			{
				logError( "Failed to create swap chain texture for eye!" );
				return 1;
			}
		}
	}


	//does oculusNUM_FRAMES change between head sets?
	// or there a constant defined in libOVR so I don't have to do this
	ovr_GetTextureSwapChainLength( oculusSession, oculusEyeSwapChains[0] , &oculusNUM_FRAMES);

	commandAllocators = (ID3D12CommandAllocator**)malloc( (oculusNUM_FRAMES*ovrEye_Count*(sizeof(ID3D12CommandAllocator*) + sizeof(ID3D12Resource*))) + sizeof(ID3D12CommandAllocator*) );
	oculusEyeBackBuffers = (ID3D12Resource**)(commandAllocators + (oculusNUM_FRAMES*ovrEye_Count) + 1);

#if MAIN_DEBUG
	s32 otherTextureCount;
	for( u32 dwEye = 1; dwEye < ovrEye_Count; ++dwEye )
	{
		ovr_GetTextureSwapChainLength( oculusSession, oculusEyeSwapChains[dwEye] , &otherTextureCount);
		assert( otherTextureCount == oculusNUM_FRAMES );
	}
#endif

	rtvDescriptorHeap = InitRenderTargetDescriptorHeap( device, oculusNUM_FRAMES*ovrEye_Count ); //change amt for debug mode
	if( !rtvDescriptorHeap )
	{
		logError( "Failed to create render target descriptor heap!\n" ); 
		return 1;
	}

	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

	{
		//Populate descriptor heap
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	
		D3D12_RENDER_TARGET_VIEW_DESC eyeRTVDesc;
    	eyeRTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    	eyeRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; //D3D12_RTV_DIMENSION_TEXTURE2DMS for MSAA
    	eyeRTVDesc.Texture2D.MipSlice = 0;
    	eyeRTVDesc.Texture2D.PlaneSlice = 0;
    	//eyeRTVDesc.Texture2DMS.UnusedField_NothingToDefine = 0; //for MSAA
	
		for(u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye)
		{
			eyeStartingRTVHandle[dwEye] = rtvHandle;
	
			for( u32 dwIdx = 0; dwIdx < (u32)oculusNUM_FRAMES; ++dwIdx )
			{
				if( ovr_GetTextureSwapChainBufferDX( oculusSession, oculusEyeSwapChains[dwEye], dwIdx, IID_PPV_ARGS(&oculusEyeBackBuffers[(dwEye*oculusNUM_FRAMES) + dwIdx])) < 0 )
				{
					logError( "Failed to get rtv handle!\n" ); 
					return 1;
				}
#if MAIN_DEBUG
				oculusEyeBackBuffers[(dwEye*oculusNUM_FRAMES) + dwIdx]->SetName(L"Eye Swap Chain Texture");
#endif
				device->CreateRenderTargetView( oculusEyeBackBuffers[(dwEye*oculusNUM_FRAMES) + dwIdx], &eyeRTVDesc, rtvHandle );
				rtvHandle.ptr = (u64)rtvHandle.ptr + rtvDescriptorSize;
			}
		}
	}

	dsDescriptorHeap = InitDepthStencilDescriptorHeap( device, ovrEye_Count );
	if( !dsDescriptorHeap )
	{
		logError( "Failed to create depth buffer descriptor heap!\n" );
		return 1;
	}

	if( !CreateDepthStencilBuffer() )
	{
		return 1;
	}

	for( u32 dwIdx = 0; dwIdx < (u32)(oculusNUM_FRAMES*ovrEye_Count+1); ++dwIdx )
	{
		if( FAILED( device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &commandAllocators[dwIdx] ) ) ) )
		{
			logError( "Failed to create command allocator!\n" );
			return 1;
		}
	}

	s32 oculusCurrentSwapChainIndex;
	ovr_GetTextureSwapChainCurrentIndex(oculusSession, oculusEyeSwapChains[0], &oculusCurrentSwapChainIndex);
#if MAIN_DEBUG
	s32 otherSwapChainIndex;
	for( u32 dwEye = 1; dwEye < ovrEye_Count; ++dwEye )
	{
		ovr_GetTextureSwapChainCurrentIndex(oculusSession, oculusEyeSwapChains[dwEye], &otherSwapChainIndex);
		assert( otherSwapChainIndex == oculusCurrentSwapChainIndex );
	}
#endif

	//create a command list for each eye plus 1 for model uploading (in the future could all be split across threads)
	for( u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye )
	{
		if( FAILED( device->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[(dwEye*oculusNUM_FRAMES)+oculusCurrentSwapChainIndex], NULL, IID_PPV_ARGS( &commandLists[dwEye] ) ) ) )
		{
			logError( "Failed to create Command list (it will change which allocator it allocates commands into every frame)!\n" );
			return 1;
		}
#if MAIN_DEBUG
		commandLists[dwEye]->SetName(L"Eye Command List");
#endif
		if( FAILED( commandLists[dwEye]->Close() ) )
		{
			logError( "Command list failed to close, go through debug layer to see what command failed!\n" );
			return 1;
		}
	}
	if( FAILED( device->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[ovrEye_Count*oculusNUM_FRAMES], NULL, IID_PPV_ARGS( &commandLists[ovrEye_Count] ) ) ) )
	{
		logError( "Failed to create Command list (it will change which allocator it allocates commands into every frame)!\n" );
		return 1;
	}
#if MAIN_DEBUG
	commandLists[ovrEye_Count]->SetName(L"Streaming Command List");
#endif

	if( FAILED( device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &streamingFence ) ) ) )
	{
		logError( "Failed to create GPU/CPU fence!\n" );
		return false;
	}

	currStreamingFenceValue = 0;
	streamingFenceValue = 0;

	fenceEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	if( !fenceEvent )
	{
		logError( "Failed to create fence event!\n" );
		return false;
	}

	UploadModels();

	if( FAILED( commandLists[ovrEye_Count]->Close() ) )
	{
		logError( "Command list failed to close, go through debug layer to see what command failed!\n" );
		return 1;
	}

	ID3D12CommandList* ppCommandLists[] = { commandLists[ovrEye_Count] };
    commandQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

    FlushStreamingCommandQueue();
    uploadBuffer->Release();
	pModelUploadHeap->Release();

	//vertex shader constants
	D3D12_ROOT_CONSTANTS cbVertDesc;
	cbVertDesc.ShaderRegister = 0;
	cbVertDesc.RegisterSpace = 0;
	                            //float4x4  //float3x3
	cbVertDesc.Num32BitValues = ( 4 * 4 ) + ( ( ( 4 * 2 ) + 3 ) );

	D3D12_ROOT_CONSTANTS cbPixelDesc;
	cbPixelDesc.ShaderRegister = 1;
	cbPixelDesc.RegisterSpace = 0;
	                            //float4 and float3
	cbPixelDesc.Num32BitValues = 4 + 3;

	D3D12_ROOT_PARAMETER rootParams[2];
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[0].Constants = cbVertDesc;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[1].Constants = cbPixelDesc;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//D3D12_VERSIONED_ROOT_SIGNATURE_DESC
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.NumParameters = 2;
	rootSignatureDesc.pParameters = rootParams;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT  | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS  | D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS; //D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS

	ID3DBlob* serializedRootSignature;
	if( FAILED( D3D12SerializeRootSignature( &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &serializedRootSignature, nullptr ) ) )
	{
		logError( "Failed to serialize root signature!\n" );
		return false;
	}

	if( FAILED( device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS( &rootSignature ) ) ) )
	{
		logError( "Failed to create root signature!\n" );
		return false;
	}
	//todo can i free serializedRootSignature here?

	//this describes the vertex inut layout (if we were doing instancing this can change to allow per instance data)

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	DXGI_SAMPLE_DESC sampleDesc;
	sampleDesc.Count = dwSampleRate;
	sampleDesc.Quality = 0; //TODO if msaa is added


	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
	inputLayoutDesc.pInputElementDescs = inputLayout;
	inputLayoutDesc.NumElements = 3;

	D3D12_SHADER_BYTECODE vertexShaderBytecode;
	vertexShaderBytecode.pShaderBytecode = vertexShaderBlob;
	vertexShaderBytecode.BytecodeLength = sizeof(vertexShaderBlob);

	D3D12_SHADER_BYTECODE pixelShaderBytecode;
	pixelShaderBytecode.pShaderBytecode = pixelShaderBlob;
	pixelShaderBytecode.BytecodeLength = sizeof(pixelShaderBlob);

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc;
	renderTargetBlendDesc.BlendEnable = 0;
	renderTargetBlendDesc.LogicOpEnable = 0;
	renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
	renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_BLEND_DESC pipelineBlendState;
	pipelineBlendState.AlphaToCoverageEnable = 0;
	pipelineBlendState.IndependentBlendEnable = 0;
	for( u32 dwRenderTarget = 0; dwRenderTarget < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++dwRenderTarget )
	{
		pipelineBlendState.RenderTarget[ dwRenderTarget ] = renderTargetBlendDesc;
	}

	D3D12_RASTERIZER_DESC pipelineRasterizationSettings;
	pipelineRasterizationSettings.FillMode = D3D12_FILL_MODE_SOLID;
	pipelineRasterizationSettings.CullMode = D3D12_CULL_MODE_BACK;
	pipelineRasterizationSettings.FrontCounterClockwise = 0;
	pipelineRasterizationSettings.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	pipelineRasterizationSettings.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	pipelineRasterizationSettings.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	pipelineRasterizationSettings.DepthClipEnable = 1;
	pipelineRasterizationSettings.MultisampleEnable = 0;
	pipelineRasterizationSettings.AntialiasedLineEnable = 0;
	pipelineRasterizationSettings.ForcedSampleCount = 0;
	pipelineRasterizationSettings.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCILOP_DESC frontFaceDesc;
	frontFaceDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	frontFaceDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	frontFaceDesc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	frontFaceDesc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_DEPTH_STENCILOP_DESC backFaceDesc;
	backFaceDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	backFaceDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	backFaceDesc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	backFaceDesc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_DEPTH_STENCIL_DESC pipelineDepthStencilState;
	pipelineDepthStencilState.DepthEnable = 1;
	pipelineDepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipelineDepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	pipelineDepthStencilState.StencilEnable = 0;
	pipelineDepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	pipelineDepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	pipelineDepthStencilState.FrontFace = frontFaceDesc;
	pipelineDepthStencilState.BackFace = backFaceDesc;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc;
	pipelineDesc.pRootSignature = rootSignature; //why is this even here if we are going to set it in the command list?
	pipelineDesc.VS = vertexShaderBytecode;
	pipelineDesc.PS = pixelShaderBytecode;
	pipelineDesc.DS = {};
	pipelineDesc.HS = {};
	pipelineDesc.GS = {};
	pipelineDesc.StreamOutput = {};
	pipelineDesc.BlendState = pipelineBlendState;
	pipelineDesc.SampleMask = 0xffffffff;
	pipelineDesc.RasterizerState = pipelineRasterizationSettings;
	pipelineDesc.DepthStencilState = pipelineDepthStencilState;
	pipelineDesc.InputLayout = inputLayoutDesc;
	pipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED ;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	for( u32 dwRenderTargetFormat = 1; dwRenderTargetFormat < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++dwRenderTargetFormat )
	{
		pipelineDesc.RTVFormats[dwRenderTargetFormat] = DXGI_FORMAT_UNKNOWN;
	}
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineDesc.SampleDesc = sampleDesc;
	pipelineDesc.NodeMask = 0;
	pipelineDesc.CachedPSO = {};
	pipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE; //set in debug mode for embedded graphics

	if( FAILED( device->CreateGraphicsPipelineState( &pipelineDesc, IID_PPV_ARGS( &pipelineStateObject ) ) ) )
	{
		logError( "Failed to create pipeline state object!\n" );
		return false;
	}

	return 0;
}

//change release to WinMainCRTStartup


void DrawScene( f32 deltaTime )
{
	ovrSessionStatus oculusSessionStatus;
    ovr_GetSessionStatus( oculusSession, &oculusSessionStatus );
    if( oculusSessionStatus.ShouldQuit )
    {
    	CloseProgram();
    	return;
    }
    if( oculusSessionStatus.ShouldRecenter )
    {
    	//TODO better
    	ovr_RecenterTrackingOrigin( oculusSession );
    }
    if( !oculusSessionStatus.HasInputFocus )
    {
    	Pause();
    	deltaTime = 0.0f;
    }
    else
    {
    	UnPause();
    }


    if( oculusSessionStatus.IsVisible )
    {
    	if( ovr_WaitToBeginFrame( oculusSession, oculusFrameIndex ) < 0 )
    	{
#if MAIN_DEBUG
    		//TODO change to a retry create head set, maybe?
    		printf("wait to begin failed\n");
#endif
    		CloseProgram();
    		return;
    	}
    	if( ovr_BeginFrame( oculusSession, oculusFrameIndex ) < 0 )
    	{
#if MAIN_DEBUG
    		//TODO change to a retry create head set, maybe?
    		printf("begin failed\n");
#endif
    		CloseProgram();
    		return;
    	}

    	//will this change? why can't i just do this once or when the HMD characterics change?
    	ovrEyeRenderDesc oculusEyeRenderDesc[ovrEye_Count];
    	ovrPosef EyeRenderPose[ovrEye_Count];
    	ovrPosef HmdToEyePose[ovrEye_Count]; //Transform of eye from the HMD center (but how to get HMD center position in absolute space?), in meters. (in other words ipd taken into account)
    	for( u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye )
    	{
    		oculusEyeRenderDesc[dwEye] = ovr_GetRenderDesc( oculusSession, (ovrEyeType)dwEye, oculusHMDDesc.DefaultEyeFov[dwEye] );
    		HmdToEyePose[dwEye] = oculusEyeRenderDesc[dwEye].HmdToEyePose;
    	}

    	//converts HMDToEye (ipd for each eye from head set center) to actual world space position's (from origin)
    	f64 fSensorSampleTime;
    	ovr_GetEyePoses( oculusSession, oculusFrameIndex, ovrTrue, HmdToEyePose, EyeRenderPose, &fSensorSampleTime );

    	//todo verify with mouse manipulation of headset view
		Quatf qHor, qVert;
		Vec3f vertAxis = {cosf(rotHor*PI_F/180.0f),0,-sinf(rotHor*PI_F/180.0f)};
		Vec3f horAxis = {0,1,0};
		InitUnitQuatf( &qVert, rotVert, &vertAxis );
		InitUnitQuatf( &qHor, rotHor, &horAxis );
	
		Quatf qRot;
		QuatfMult( &qVert, &qHor, &qRot);

    	Mat4f mTrans;
    	InitTransMat4f( &mTrans, 0, 0, -5); //TODO should this be negative or the view matrix position be negated?
    	Mat4f mRot;
    	Vec3f rotAxis = {0.57735026919f,0.57735026919f,0.57735026919f};
    	static f32 cubeRotAngle = 0;
    	cubeRotAngle += 50.0f*deltaTime;
    	InitRotArbAxisMat4f( &mRot, &rotAxis, cubeRotAngle );
    	Mat4f mCubeModel;
    	Mat4fMult(&mRot, &mTrans, &mCubeModel);

		Mat4f mPlaneModel;
		InitMat4f( &mPlaneModel );

    	for( u32 dwEye = 0; dwEye < ovrEye_Count; ++dwEye )
    	{
    		//why would the following be different per eye?
    		Quatf eyeQuat;
    		eyeQuat.w = EyeRenderPose[dwEye].Orientation.w;
    		eyeQuat.x = EyeRenderPose[dwEye].Orientation.x;
    		eyeQuat.y = EyeRenderPose[dwEye].Orientation.y;
    		eyeQuat.z = EyeRenderPose[dwEye].Orientation.z;

    		Vec3f eyePos;
    		eyePos.x = EyeRenderPose[dwEye].Position.x; 
    		eyePos.y = EyeRenderPose[dwEye].Position.y;
    		eyePos.z = EyeRenderPose[dwEye].Position.z;

			Quatf eyeCamRot;
			QuatfMult( &eyeQuat, &qRot, &eyeCamRot );

			Vec3f vRotatedEyePos;
    		Vec3fRotByUnitQuat(&eyePos,&qRot,&vRotatedEyePos);
    		Vec3f eyeCamPos;
    		Vec3fAdd( &vRotatedEyePos, &startingPos, &eyeCamPos );

    		s32 swapChainIndex = 0;
        	ovr_GetTextureSwapChainCurrentIndex(oculusSession, oculusEyeSwapChains[dwEye], &swapChainIndex); //I don't think this will ever be out of sync between swap chains...

        	commandAllocators[(dwEye*oculusNUM_FRAMES) + swapChainIndex]->Reset();
			commandLists[dwEye]->Reset( commandAllocators[(dwEye*oculusNUM_FRAMES) + swapChainIndex], pipelineStateObject );

    		D3D12_RESOURCE_BARRIER presentToRenderBarrier;
    		presentToRenderBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    		presentToRenderBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    		presentToRenderBarrier.Transition.pResource = oculusEyeBackBuffers[(dwEye*oculusNUM_FRAMES) + swapChainIndex];
   			presentToRenderBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    		presentToRenderBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    		presentToRenderBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    		commandLists[dwEye]->ResourceBarrier( 1, &presentToRenderBarrier );
    		
    		//render here
    		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = eyeStartingRTVHandle[dwEye];
    		rtvHandle.ptr = (u64)rtvHandle.ptr + ( rtvDescriptorSize * swapChainIndex );
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = eyeDSVHandle[dwEye]; //need 2 textures cause they may be diff sizes
		
			commandLists[dwEye]->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		
    		const float clearColor[] = { 0.5294f, 0.8078f, 0.9216f, 1.0f };
    		commandLists[dwEye]->ClearRenderTargetView( rtvHandle, clearColor, 0, NULL );
    		commandLists[dwEye]->ClearDepthStencilView( dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
    		
    		commandLists[dwEye]->SetGraphicsRootSignature( rootSignature ); //does this always need to be set?
			commandLists[dwEye]->SetGraphicsRoot32BitConstants( 1, 4 + 3, &pixelConstantBuffer ,0);

			commandLists[dwEye]->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST ); 

    		commandLists[dwEye]->RSSetViewports( 1, &EyeViewports[dwEye] ); //does this always need to be set?
    		commandLists[dwEye]->RSSetScissorRects( 1, &EyeScissorRects[dwEye] ); //does this always need to be set?

			Mat4f mView;
    		InitViewMat4ByQuatf( &mView, &eyeCamRot, &eyeCamPos );
		
			Mat4f mProj;
			InitPerspectiveProjectionMat4fOculusDirectXRH( &mProj, oculusEyeRenderDesc[dwEye].Fov, 0.2f, 100.0f );

    		Mat4f mVP;
    		Mat4fMult( &mView, &mProj, &mVP);
		
    		Mat4fMult(&mPlaneModel,&mVP, &vertexConstantBuffer.mvpMat);
    		InverseTransposeUpper3x3Mat4f( &mPlaneModel, &vertexConstantBuffer.nMat );
		
    		commandLists[dwEye]->SetGraphicsRoot32BitConstants( 0, ( 4 * 4 ) + ( ( ( 4 * 2 ) + 3 ) ), &vertexConstantBuffer ,0);
    		commandLists[dwEye]->IASetVertexBuffers( 0, 1, &planeVertexBufferView );
    		commandLists[dwEye]->IASetIndexBuffer( &planeIndexBufferView );
    		commandLists[dwEye]->DrawIndexedInstanced( planeIndexCount, 1, 0, 0, 0 );
		
		    Mat4fMult(&mCubeModel,&mVP, &vertexConstantBuffer.mvpMat);
    		InverseTransposeUpper3x3Mat4f( &mCubeModel, &vertexConstantBuffer.nMat );
		
    		commandLists[dwEye]->SetGraphicsRoot32BitConstants( 0, ( 4 * 4 ) + ( ( ( 4 * 2 ) + 3 ) ), &vertexConstantBuffer ,0);
    		commandLists[dwEye]->IASetVertexBuffers( 0, 1, &cubeVertexBufferView );
    		commandLists[dwEye]->IASetIndexBuffer( &cubeIndexBufferView );
    		commandLists[dwEye]->DrawIndexedInstanced( cubeIndexCount, 1, 0, 0, 0 );
		
    		D3D12_RESOURCE_BARRIER renderToPresentBarrier;
    		renderToPresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    		renderToPresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    		renderToPresentBarrier.Transition.pResource = oculusEyeBackBuffers[(dwEye*oculusNUM_FRAMES) + swapChainIndex];
   			renderToPresentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    		renderToPresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    		renderToPresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    		commandLists[dwEye]->ResourceBarrier( 1, &renderToPresentBarrier );

    		if( FAILED( commandLists[dwEye]->Close() ) )
			{
				logError( "Command list failed to close, go through debug layer to see what command failed!\n" );
				CloseProgram();
				return;
			}
			
			ID3D12CommandList* ppCommandLists[] = { commandLists[dwEye] };
    		commandQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

    		ovr_CommitTextureSwapChain( oculusSession, oculusEyeSwapChains[dwEye]); //does this muck with the command list/command queue?
    	}

    	//We specify the layer information now for the compositor, in the future use ovrLayerEyeFovDepth for asyn time warping
    	//They use the depth version of ovrLayerEyeFov_ instead to do positional timewarp, but for our example, do we even need positional timewarp?
    	ovrLayerEyeFov ld;
    	ld.Header.Type = ovrLayerType_EyeFov; //look into ovrLayerType
    	ld.Header.Flags = 0; //look into ovrLayerFlags
    	memset(ld.Header.Reserved,0,128);
    	ld.SensorSampleTime = fSensorSampleTime;
    	for (int dwEye = 0; dwEye < ovrEye_Count; ++dwEye)
    	{
    	    ld.ColorTexture[dwEye] = oculusEyeSwapChains[dwEye];
    	    ld.Viewport[dwEye] = oculusEyeRenderViewport[dwEye];
    	    ld.Fov[dwEye] = oculusHMDDesc.DefaultEyeFov[dwEye];
    	    ld.RenderPose[dwEye] = EyeRenderPose[dwEye];
    	}

    	ovrLayerHeader* oculusLayers = &ld.Header;
    	if( ovr_EndFrame( oculusSession, oculusFrameIndex, nullptr, &oculusLayers, 1 ) < 0 )
    	{
#if MAIN_DEBUG
    		//TODO change to a retry create head set, maybe?
    		printf("end failed\n");
#endif
    		CloseProgram();
    	}
    	++oculusFrameIndex;
    }
}


#if MAIN_DEBUG
s32 main()
#else
s32 APIENTRY WinMain(
    _In_ HINSTANCE Instance,
    _In_opt_ HINSTANCE PrevInstance,
    _In_ LPSTR CommandLine,
    _In_ s32 ShowCode )
#endif
{
	//TODO enter a searching for headset loop, once head set is found initialize it.
	//     then go into that rendering loop for that headset
	//     if that headset is disconnected for any reason go back to the searching for headset loop
	//     that way we can have special rendering loops for each type of hardware and even handle the user unplugging their headset
	//		have a little on screen window with one of those waiting spinner icons saying searching for headset
	//		also figure out minimal recreate. Do we need to reupload models and all that? Maybe for some headset but not others? or are they only on GPUs, so then no worry?
	//		pause the game too! 
	//TODO handle GPU device lost! If there is headset find GPU with headset attachted, (following is not our situation)If there is no headset Swap to next user preferred GPU or integrated graphics if they have none
	ovrInitParams oculusInitParams = { ovrInit_RequestVersion | ovrInit_FocusAware, OVR_MINOR_VERSION, NULL, 0, 0 };
	if( ovr_Initialize( &oculusInitParams ) >= 0 ) //can this persist outside of loop when trying to recreate headset? or does this need to be in retry create loop?
	{
		InitStartingCamera();
		if( InitOculusHeadset() != 0 )
		{
			ovr_Shutdown(); //how to handle this on retry create on headset unplugged 
			return -1;
		}

		LARGE_INTEGER PerfCountFrequencyResult;
    	QueryPerformanceFrequency( &PerfCountFrequencyResult );
    	int64_t PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    	LARGE_INTEGER LastCounter;
    	QueryPerformanceCounter( &LastCounter );

		InitStartingGameState();
		InitHeadsetGraphicsState();
		if( InitDirectX12() )
		{
			ovr_Destroy( oculusSession );
			ovr_Shutdown();
			return -1;
		}

		while( Running )
		{
    		u64 EndCycleCount = __rdtsc();
    	
    		LARGE_INTEGER EndCounter;
    		QueryPerformanceCounter(&EndCounter);
    	
    		//Display the value here
    		s64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
    		f32 deltaTime = CounterElapsed / (f32)PerfCountFrequency ;
    		f64 MSPerFrame = (f64) ( ( 1000.0f * CounterElapsed ) / (f64)PerfCountFrequency );
    		f64 FPS = PerfCountFrequency / (f64)CounterElapsed;
    		LastCounter = EndCounter;

#if MAIN_DEBUG
    		//char buf[64];
    		//sprintf_s( &buf[0], 64, "%s: fps %f", WindowName, FPS );
    		//SetWindowTextA( MainWindowHandle, &buf[0] );
#endif


			MSG Message;
        	while( PeekMessage( &Message, 0, 0, 0, PM_REMOVE ) )
        	{
        		switch( Message.message )
        		{
        	        case WM_QUIT:
        	        {
        	        	CloseProgram();
        	        	break;
        	        }
        	        case WM_SYSKEYDOWN:
        	        case WM_SYSKEYUP:
        	        case WM_KEYDOWN:
        	        case WM_KEYUP:
        	        {
        	           uint32_t VKCode = (uint32_t) Message.wParam;
        	           bool WasDown = ( Message.lParam & ( 1 << 30 ) ) != 0;
        	           bool IsDown = ( Message.lParam & ( 1 << 31 ) ) == 0;
        	           bool AltKeyWasDown = ( Message.lParam & ( 1 << 29 ) );
        	           switch( VKCode )
        	           {
        	                case VK_ESCAPE:
        	                {
        	                	if( WasDown != 1 && WasDown != IsDown )
        	                	{
        	                		TogglePause();
        	                	}
        	                	break;
        	                }
        	                case VK_F4:
        	                {
        	                	if( AltKeyWasDown )
        	                	{
        	                		CloseProgram();
        	                	}
        	                	break;
        	                }
        	                default:
        	                {
        	                	break;
        	                }
        	            }
        	            break;
        	        }
        	        default:
        	        {
        	        	TranslateMessage( &Message );
        	        	DispatchMessage( &Message );
        	        	break;
        	        }
            	}
        	}

        	//TODO MOVE OCULUS SESSION STATUS OUTSIDE DRAW SCENE AND MOVE IF STATEMENTS OUTSIDE OF IT
        	//DEAL WITH OCULUS CONTEXT LOST LIKE DEMO
        	DrawScene( ( 1 - isPaused ) * deltaTime );
		}
		//free(commandAllocators);
		ovr_Destroy( oculusSession );
		ovr_Shutdown();
	}

	return 0;
}