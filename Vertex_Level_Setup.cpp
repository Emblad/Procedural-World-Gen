#include	<tgSystem.h>

#include	"CLevel.h"

#include	"PerlinNoise.h"

//needs to disable the tengine system to use theese functions
#include <tgMemoryDisable.h>
#include	<vector>
#include	<math.h>
#include <tgMemoryEnable.h>

#include  <tgCCommandQueue.h>

#include "Renderer/CRenderCallBacks.h"
#include "Camera/CCamera.h"


#define clamp(value,minimum,maximum) max(min(value,maximum),minimum)

////////////////////////////// CLevel //////////////////////////////
//                                                                //
//  Info:
//                                                                //
//*/////////////////////////////////////////////////////////////////
CLevel::CLevel(void)
	:m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pConstantBuffer(nullptr)
	, m_pVertexBuffer(nullptr)
	, m_pIndexBuffer(nullptr)
	, m_pInputLayout(nullptr)
	, terrainSize(25)
	, amplitude(4.4)
	, frequency(0.055)
	, heightMap(0)
	, octaves(15)
	, terracing_multiplier(150)
	, power(1.5f)
{
	//Load worlds
	CWorldManager& rWorldManager = CWorldManager::GetInstance();
	m_pWorld = rWorldManager.LoadWorld("", "Level");

	m_pCollisionWorld = rWorldManager.LoadWorld("worlds/demo_level_collision.tfw", "Collision");
	rWorldManager.SetActiveWorld(m_pWorld);

	for (tgUInt32 SectorIndex = 0; SectorIndex < m_pWorld->GetNumSectors(); ++SectorIndex)
	{
		tgSWorldSector& rSector = *m_pWorld->GetSector(SectorIndex);
		for (tgUInt32 MeshIndex = 0; MeshIndex < rSector.NumMeshes; ++MeshIndex)
		{
			tgCMesh& rMesh = rSector.pMeshArray[MeshIndex];

			if (strncmp(rMesh.GetAssetName().String(), "TileGround_", 11) == 0)
			{
				for (tgUInt32 SubMeshIndex = 0; SubMeshIndex < rMesh.GetNumSubMeshes(); ++SubMeshIndex)
				{
					tgCMesh::SSubMesh& rSubMesh = *rMesh.GetSubMesh(SubMeshIndex);
					tgCTexture* pNormalmap = rSubMesh.Material.GetNormalmap();
					tgCTextureManager::GetInstance().Destroy(&pNormalmap);
					pNormalmap = tgCTextureManager::GetInstance().Create("tg_embedded_lightblue");
					rSubMesh.Material.SetNormalmap(*pNormalmap);
				}
			}
		}
	}

	//Buffer
	SConstantPixel sConstatntPixel;

	vertexCount = resolution * resolution;
	vertex = new  SVertex[vertexCount];

	m_pVertexBuffer = new tgCVertexBuffer("Ground Vertex Buffer", 0, sizeof(SVertex), nullptr, false);
	indexCount = ((resolution - 1) * (resolution - 1)) * 6;
	Indices = new tgUInt32[indexCount];

	m_pIndexBuffer = new tgCIndexBuffer("Ground  Index  Buffer", 0, sizeof(tgUInt32), nullptr, false);

	m_pConstantBuffer = new tgCConstantBuffer("Ground Constant Buffer", 0, nullptr, false);
	m_pConstantPixelBuffer = new tgCConstantBuffer("Constant Buffer Pixel: Level", 0, nullptr, false);
	m_pVertexShader = tgCShaderManager::GetInstance().Create("shaders/d3d11/mesh_custom_ambient_vertex", tgCShader::TYPE_VERTEX);
	m_pPixelShader = tgCShaderManager::GetInstance().Create("shaders/d3d11/mesh_custom_ambient_pixel", tgCShader::TYPE_PIXEL);

	//sConstatntPixel.u_HeightColor = tgCColor::Red;

	tgCCommandQueue::GetInstance().Sync();

#if defined(TG_D3D11)
	D3D11_INPUT_ELEMENT_DESC InputDescripotors[2]
	{
		{"POSITION",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 0,	D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",			0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA, 0 },

	};
	ID3D11Device* pDevice = tgCD3D11::GetInstance().LockDevice();
	
	pDevice->CreateInputLayout(InputDescripotors, 2, m_pVertexShader->GetData(), m_pVertexShader->GetDataSize(), &m_pInputLayout);

	tgCD3D11::GetInstance().UnlockDevice();

	CreateHeightMap();
	
	AddPerlinNoise(amplitude, frequency);
	BrownianMotion(octaves, amplitude, frequency);

	RigidNoise(amplitude, frequency);
	InverseRigidNoise(amplitude, frequency);
	
	Terrace(terracing_multiplier);
	
	AddPerlinNoise(amplitude, frequency);
	BrownianMotion(octaves, amplitude, frequency);
	
	Redistribution(power);
	Smoothing();


#endif
}	

CLevel::~CLevel(void)
{
	CWorldManager::GetInstance().DestroyWorld(m_pCollisionWorld);
	CWorldManager::GetInstance().DestroyWorld(m_pWorld);
	delete[] heightMap;
	heightMap = 0;

}

void
CLevel::CreateHeightMap() {
	heightMap = new float[resolution * resolution];
	float height = 0.0f;
	const float scale = terrainSize / static_cast<float>(resolution);

	for (int j = 0; j < (resolution); j++)
	{
		for (int i = 0; i < (resolution); i++)
		{
			height = 1;
			heightMap[(j * resolution) + i] = height;
		}
	}
}
//fault line
void CLevel::FaultLine()
{
	float  x1;
	float  x2;

	float  y1;
	float  y2;

	x1 = (float)(rand() % 10000 / 10000.f) * resolution;
	x2 = (float)(rand() % 10000 / 10000.f) * resolution;

	y1 = (float)(rand() % 10000 / 10000.f) * resolution;
	y2 = (float)(rand() % 10000 / 10000.f) * resolution;

	tgCV3D line = tgCV3D(x2 - x1, 0, y2 - y1);

		float height_increase = 5.f - ((float)(rand() % 100) / 100) * 10.f;

		for (int y = 0; y < (resolution); y++)
		{
			for (int x = 0; x < (resolution); x++)
			{

				tgCV3D point = tgCV3D(x1 - x, 0, y1 - y);

				tgCV3D cross = CrossProd(line, point);

				if (cross.y > 0)
				{
					heightMap[(y * resolution) + x] += height_increase;
				}
			}
		}
		CreateVertex();
}
tgCV3D
CLevel::CrossProd(tgCV3D line, tgCV3D point) {
	tgCV3D cross;
	cross.x = line.y * point.z - line.z * point.y;
	cross.y = line.z * point.x - line.x * point.z;
	cross.z = line.x * point.y - line.y * point.x;

	return cross;
}
void CLevel::AddPerlinNoise(float amplitude, float frequency)
{
	const float scale = terrainSize / static_cast<float>(resolution);
	for (int j = 0; j < (resolution); j++)
	{
		for (int i = 0; i < (resolution); i++)
		{

			float twoPoints[2] = { (float)i * frequency * scale,(float)j * frequency * scale };

			heightMap[(j * resolution) + i] += PerlinNoise::Noise2(twoPoints) * amplitude;
		}
	}
	CreateVertex();


}
void CLevel::BrownianMotion(int octaves, float amplitude, float frequency)
{
	for (size_t i = 0; i < octaves; i++)
	{
		AddPerlinNoise(amplitude, frequency);
		amplitude *= 0.5f;
		frequency *= 2;
	}
	CreateVertex();

}
void CLevel::RigidNoise(float amplitude, float frequency)
{
	const float scale = terrainSize / static_cast<float>(resolution);

	for (int j = 0; j < (resolution); j++)
	{
		for (int i = 0; i < (resolution); i++)
		{
			float twoPoints[2] = { (float)i * frequency * scale,(float)j * frequency * scale };
			heightMap[(j * resolution) + i] += -(1.0f - abs(PerlinNoise::Noise2(twoPoints) * amplitude));
		}
	}
	CreateVertex();

}
void CLevel::InverseRigidNoise(float amplitude, float frequency)
{
	const float scale = terrainSize / static_cast<float>(resolution);

	for (int j = 0; j < (resolution); j++)
	{
		for (int i = 0; i < (resolution); i++)
		{
			float twoPoints[2] = { (float)i * frequency * scale,(float)j * frequency * scale };
			heightMap[(j * resolution) + i] += (1.0f - abs(PerlinNoise::Noise2(twoPoints) * amplitude));
		}
	}
	CreateVertex();

}
void CLevel::Terrace(float terracing_multiplier)
{
	const float scale = terrainSize / static_cast<float>(resolution);

	for (int j = 0; j < (resolution); j++) // loop for resolution - y
	{
		for (int i = 0; i < (resolution); i++) // loop for resolution - x
		{
			heightMap[(j * resolution) + i] = round(heightMap[(j * resolution) + i]) / terracing_multiplier;
		}
	}
	CreateVertex();
}
void CLevel::Redistribution(float power)
{
	const float scale = terrainSize / static_cast<float>(resolution);
	float noise_value;
	for (int j = 0; j < (resolution); j++) // loop for resolution - y
	{
		for (int i = 0; i < (resolution); i++)
		{
			noise_value = (abs(heightMap[(j * resolution) + i]));
			heightMap[(j * resolution) + i] = pow((noise_value), power);

		}
	}
	CreateVertex();

}

void CLevel::Smoothing()
{
	float* copyHeight = heightMap;

	for (size_t y = 0; y < resolution; y++)
	{
		for (size_t x = 0; x < resolution; x++)
		{
			float average = 0.f;
			int values_used = 4;
			//get the 4  values sorruinding this vertex
			int up = y + 1;
			int down = y - 1;

			int left = x - 1;
			int right = x + 1;

			up = clamp(up, 0, resolution - 1);
			down = clamp(down, 0, resolution - 1);
			left = clamp(left, 0, resolution - 1);
			right = clamp(right, 0, resolution - 1);

			average += heightMap[(up * resolution) + x];
			average += heightMap[(down * resolution) + x];
			average += heightMap[(y * resolution) + left];
			average += heightMap[(y * resolution) + right];

			average = average / 4;

			copyHeight[(y * resolution) + x] = average;
		}
	}
	heightMap = copyHeight;
	CreateVertex();
}


void CLevel::CreateVertex()
{
	int index = 0, i, j;
	float positionX, height = 1, positionZ, u, v, increment;

	const float scale = terrainSize / static_cast<float>(resolution);
	increment = m_UVscale / resolution;

	//vertex setup
	for (j = 0; j < (resolution); j++) {
		for (i = 0; i < (resolution); i++)
		{
			positionX = i * scale;
			positionZ = (j)*scale;

			height = heightMap[index];
			vertex[index].Position = { tgCV3D(positionX, height , positionZ) };

			//Sets up color based on height
			if (vertex[index].Position.y >= 12)
				vertex[index].Color = tgCV3D(0.5, 0.0, 0.0);

			else if (vertex[index].Position.y >= 8 ) {
				vertex[index].Color = tgCV3D(0.1,0.1,0.1);
			}
			 if (vertex[index].Position.y >= 1)
				vertex[index].Color = tgCV3D(0.2, 0, 0.5);

			
			else if (vertex[index].Position.y >= 0)
				vertex[index].Color = tgCV3D(3,3,0);

			else
				vertex[index].Color = tgCV3D(0, 0, 0);

			vertex[index].Color = tgCV3D(vertex[index].Color * vertex[index].Position.y / 8);

			constantsVertex.u_HeightColor.push_back(tgCV4D(vertex[index].Color, 1));

			index++;
		}
	}

	//IndexSetup
	index = 0;
	for (j = 0; j < (resolution - 1); j++) {
		for (i = 0; i < (resolution - 1); i++) {
			Indices[index] = (j * resolution) + i;
			Indices[index + 1] = ((j + 1) * resolution) + (i + 1);
			Indices[index + 2] = ((j + 1) * resolution) + i;

			Indices[index + 3] = (j * resolution) + i;
			Indices[index + 4] = (j * resolution) + (i + 1);
			Indices[index + 5] = ((j + 1) * resolution) + (i + 1);
			index += 6;
		}
	}

	//SetUpNormals
	for (j = 0; j < (resolution - 1); j++)
	{
		for (i = 0; i < (resolution - 1); i++)
		{
			tgCV3D a, b, c;
			a = vertex[j * resolution + i].Position;

			b = vertex[j * resolution + i + 1].Position;

			c = vertex[(j + 1) * resolution + i].Position;

			tgCV3D ac(c.x - a.x, c.y - a.y, c.z - a.z);
			tgCV3D ab(b.x - a.x, b.y - a.y, b.z - a.z);

			tgCV3D cross;
			cross.x = ac.y * ab.z - ac.z * ab.y;
			cross.y = ac.z * ab.x - ac.x * ab.z;
			cross.z = ac.x * ab.y - ac.y * ab.x;

			float mag = (cross.x * cross.x) + (cross.y * cross.y) + (cross.z * cross.z);
			mag = sqrtf(mag);
			cross.x /= mag;
			cross.y /= mag;
			cross.z /= mag;
			vertex[j * resolution + i].normalize(vertex[j * resolution + i].Position) = cross;


		}

	}
	tgCV3D smoothedNormal(0, 1, 0);
	for (j = 0; j < resolution; j++) {
		for (i = 0; i < resolution; i++) {
			smoothedNormal.x = 0;
			smoothedNormal.y = 0;
			smoothedNormal.z = 0;
			float count = 0;
			////Left planes
			if ((i - 1) >= 0) {
				//Top planes
				if ((j) < (resolution - 1)) {
					smoothedNormal.x += vertex[j * resolution + (i - 1)].normalize(vertex[j * resolution + i].Position).x;
					smoothedNormal.y += vertex[j * resolution + (i - 1)].normalize(vertex[j * resolution + i].Position).y;
					smoothedNormal.z += vertex[j * resolution + (i - 1)].normalize(vertex[j * resolution + i].Position).z;
					count++;
				}
				//Bottom planes
				if ((j - 1) >= 0) {
					smoothedNormal.x += vertex[(j - 1) * resolution + (i - 1)].normalize(vertex[j * resolution + i].Position).x;
					smoothedNormal.y += vertex[(j - 1) * resolution + (i - 1)].normalize(vertex[j * resolution + i].Position).y;
					smoothedNormal.z += vertex[(j - 1) * resolution + (i - 1)].normalize(vertex[j * resolution + i].Position).z;
					count++;
				}
			}
			//right planes
			if ((i) < (resolution - 1)) {

				//Top planes
				if ((j) < (resolution - 1)) {
					smoothedNormal.x += vertex[j * resolution + i].normalize(vertex[j * resolution + i].Position).x;
					smoothedNormal.y += vertex[j * resolution + i].normalize(vertex[j * resolution + i].Position).y;
					smoothedNormal.z += vertex[j * resolution + i].normalize(vertex[j * resolution + i].Position).z;
					count++;
				}
				//Bottom planes
				if ((j - 1) >= 0) {
					smoothedNormal.x += vertex[(j - 1) * resolution + i].normalize(vertex[j * resolution + i].Position).x;
					smoothedNormal.y += vertex[(j - 1) * resolution + i].normalize(vertex[j * resolution + i].Position).y;
					smoothedNormal.z += vertex[(j - 1) * resolution + i].normalize(vertex[j * resolution + i].Position).z;
					count++;
				}
			}
			smoothedNormal.x /= count;
			smoothedNormal.y /= count;
			smoothedNormal.z /= count;

			float mag = sqrt((smoothedNormal.x * smoothedNormal.x) + (smoothedNormal.y * smoothedNormal.y) + (smoothedNormal.z * smoothedNormal.z));
			smoothedNormal.x /= mag;
			smoothedNormal.y /= mag;
			smoothedNormal.z /= mag;

			vertex[j * resolution + i].normalize(vertex[j * resolution + i].Position) = smoothedNormal;
		}
	}
	m_pVertexBuffer->Update(vertexCount, sizeof(SVertex), vertex);

}



void CLevel::SetUpBuffer()
{

	if (m_pPixelShader && m_pVertexShader) {
		const tgCCamera* pCamera = tgCCameraManager::GetInstance().GetCurrentCamera();
		const CRenderCallBacks& rRenderCallBacks = CRenderCallBacks::GetInstance();

		tgCD3D11& rD3D11 = tgCD3D11::GetInstance();

		constantsVertex.u_MatrixWorldViewProj = pCamera->GetViewProjectionMatrix();


		m_pConstantBuffer->Update(sizeof(constantsVertex), &constantsVertex);
		m_pIndexBuffer->Update(indexCount, sizeof(tgUInt32), Indices);


		ID3D11Buffer* pVertexBuffers[1] = { m_pVertexBuffer->GetBuffer() };
		ID3D11Buffer* pConstantBuffers[1] = { m_pConstantBuffer->GetBuffer() };
		ID3D11Buffer* pConstanPixelBuffers[1] = { m_pConstantPixelBuffer->GetBuffer() };
		ID3D11Buffer* pIndexBuffer = m_pIndexBuffer->GetBuffer();
		const tgUInt32				VertexStrides[1] = { m_pVertexBuffer->GetVertexSize() };
		const tgUInt32				VertexOffsets[1] = { 0 };
		const DXGI_FORMAT			IndexFormat = m_pIndexBuffer->GetFormat();

    //locks the tengine system so that we can use D3d11
		ID3D11DeviceContext* pDeviceContext = tgCD3D11::GetInstance().LockDeviceContext();

    pDeviceContext->IASetInputLayout(m_pInputLayout);
		pDeviceContext->IASetVertexBuffers(0, 1, pVertexBuffers, VertexStrides, VertexOffsets);
		pDeviceContext->IASetIndexBuffer(pIndexBuffer, IndexFormat, 0);
		pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pDeviceContext->VSSetShader(m_pVertexShader->GetVertexShader(), NULL, 0);
		pDeviceContext->PSSetShader(m_pPixelShader->GetPixelShader(), NULL, 0);
		pDeviceContext->VSSetConstantBuffers(0, 1, std::begin(pConstantBuffers));
		pDeviceContext->PSSetConstantBuffers(0, 1, std::begin(pConstanPixelBuffers));



		pDeviceContext->DrawIndexed(m_pIndexBuffer->GetNumIndices(), 0, 0);


		tgCD3D11::GetInstance().UnlockDeviceContext();
	}
}
