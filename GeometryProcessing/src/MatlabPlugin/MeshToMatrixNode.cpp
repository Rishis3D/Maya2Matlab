#include "StdAfx.h"
#include "MeshToMatrixNode.h"





const char* MeshToMatrixNode::m_inMeshName[2]			= {"inputMesh", "inMesh"};
const char* MeshToMatrixNode::m_vertexMatrixName[2]		= {"vertexMatrix", "vertMat"}; 
const char* MeshToMatrixNode::m_VEIncidenceMatrixName[2]= {"vertexEdgeIncidenceMatrix","VEIncidenceMat"};
const char* MeshToMatrixNode::m_VFIncidenceMatrixName[2]= {"vertexFaceIncidenceMatrix","VFIncidenceMat"};
// const char* MeshToMatrixNode::m_EFIncidentMatrixName = {"edge-faceIncidentMatrix","EFIncidentMat"};
const char* MeshToMatrixNode::m_maxCurDirMatrixName[2]	= {"maxCurvatureDirection","maxCurDir"};
const char* MeshToMatrixNode::m_minCurDirMatrixName[2]	= {"minCurvatureDirection","minCurDir"};
const char* MeshToMatrixNode::m_maxCurMatrixName[2]		= {"maxCurvature","maxCurvature"};
const char* MeshToMatrixNode::m_minCurMatrixName[2]		= {"minCurvature","minCurvature"};
const char* MeshToMatrixNode::m_normalMatrixName[2]		= {"avgNormalMatrix","avgNorMat"};
const char* MeshToMatrixNode::m_bitangentMatrixName[2]  = {"bitangentMatrix","bitanMatrix"};
const char* MeshToMatrixNode::m_tangentMatrixName[2]    = {"tangentMatrix","tanMatrix"};
const char* MeshToMatrixNode::m_spaceName[2]			= {"space", "space"};
const char* MeshToMatrixNode::m_transformMatrixName[2]  = {"transformMatrix", "transMat"};
const char* MeshToMatrixNode::m_adjacencyMatrixName[2]  = {"adjacencyMatrix", "adjMat"};
const char* MeshToMatrixNode::m_cotLaplacianMatrixName[2]={"cotLaplacianMatrix", "cotLapMat"};
const char* MeshToMatrixNode::m_voronoiAreaName[2]      = {"voronoiArea", "vorArea"};


MObject MeshToMatrixNode::m_transformMatrix;
MObject MeshToMatrixNode::m_space;
MObject MeshToMatrixNode::m_vertexMatrix;
MObject MeshToMatrixNode::m_inMesh;
MObject MeshToMatrixNode::m_VEIncidenceMatrix;
MObject MeshToMatrixNode::m_VFIncidenceMatrix;
//MObject MeshToMatrixNode::m_EFIncidentMatrix;
MObject MeshToMatrixNode::m_minCurDirMatrix;
MObject MeshToMatrixNode::m_maxCurDirMatrix;
MObject	MeshToMatrixNode::m_minCurMatrix;
MObject MeshToMatrixNode::m_maxCurMatrix;
MObject MeshToMatrixNode::m_normalMatrix;
MObject MeshToMatrixNode::m_adjacencyMatrix;
MObject MeshToMatrixNode::m_bitangentMatrix;
MObject MeshToMatrixNode::m_tangentMatrix;
MObject	MeshToMatrixNode::m_cotLaplacianMatrix;
MObject	MeshToMatrixNode::m_voronoiArea;

const char* MeshToMatrixNode::m_nodeName = NODE_MESH_TO_MATRIX_NAME;
MTypeId     MeshToMatrixNode::m_id(NODE_MESH_TO_MATRIX_ID);

MeshToMatrixNode::MeshToMatrixNode(void)
{
}

MeshToMatrixNode::~MeshToMatrixNode(void)
{
}

MStatus MeshToMatrixNode::compute( const MPlug& plug, MDataBlock& data )
{
	MStatus s;
	MDataHandle inMeshData =  data.inputValue( m_inMesh, &s );
	CHECK_STATUS(s);
	MObject mesh = inMeshData.asMesh();
	if (plug == m_vertexMatrix)
	{
		computeVertexMatrix(mesh);
	}
	else if (plug == m_normalMatrix)
	{
		computeLocalFrameMatrix(mesh);
	}
	else if (plug == m_VEIncidenceMatrix)
	{
		computeVEIncidenceMatrix(mesh);
	}
	else if (plug == m_VFIncidenceMatrix)
	{
		computeVFIncidenceMatrix(mesh);
	}
	else if (plug == m_adjacencyMatrix)
	{
		computeAdjacencyMatrix(mesh);
	}
	else if (plug == m_minCurDirMatrix || plug == m_maxCurDirMatrix ||
			 plug == m_minCurMatrix	   || plug == m_maxCurMatrix)
	{
		computePrincipleCurvature(mesh);
	}
	else if (plug == m_cotLaplacianMatrix || plug == m_voronoiArea)
	{
		computeCotLaplacian(mesh);
	}
	else{
		return MS::kUnknownParameter;
	}
	data.setClean(plug);
	return MS::kSuccess;
}

MStatus MeshToMatrixNode::computeVertexMatrix(MObject& meshObj)
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s); 

	const int nVertices = meshFn.numVertices(&s);
	if (nVertices <= 0)
		return MS::kFailure;
	
	// get transform matrix
	MPlug transMatPlug = Global::getPlug(this, m_transformMatrixName[0]);
	MObject transMatObject = transMatPlug.asMObject();
	MFnMatrixData transMatData(transMatObject);
	MMatrix transMat = transMatData.matrix();
	MSpace::Space space = getCurrentSpace();
	if (space == MSpace::kObject)
		transMat.setToIdentity();

	// fill vertex array	
	double* vertArray = new double[nVertices*3];
	MItMeshVertex it(meshObj, &s);
	for (int ithVert=0; !it.isDone(&s); it.next(),ithVert++)
	{
		MPoint p = it.position(space, &s)*transMat;
		vertArray[ithVert] = p.x;
		vertArray[ithVert + nVertices] = p.y;
		vertArray[ithVert + nVertices + nVertices] = p.z;
	}

	// create matlab matrix obj
	MFnPluginData valFn;
	MObject valObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	pMat->getMatrix().setDenseDataPtr(vertArray, nVertices, 3, MatrixData::DT_DOUBLE);

	// set object
	MPlug vertMatPlug = getPlug(m_vertexMatrixName[0]);
	vertMatPlug.setValue(valObj);

	return s;
}

MStatus MeshToMatrixNode::computeAdjacencyMatrix( MObject& meshObj )
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s);

	const int nVertices = meshFn.numVertices(&s);
	const int nEdges    = meshFn.numEdges(&s);
	if (nVertices <= 0 || nEdges <= 0)
		return MS::kFailure;

	// fill vertex array
	unsigned *   dataArray = new unsigned[nEdges*2];
	mwIndex   *   rowIDArray= new mwIndex[nEdges*2];
	mwIndex   *   colRangeArray = new mwIndex[nVertices+1];
	MItMeshVertex it(meshObj, &s);
	for (int ithVertex=0, ithElement = 0; !it.isDone(&s); it.next(),ithVertex++)
	{
		MIntArray adjID;
		it.getConnectedVertices(adjID);

		colRangeArray[ithVertex] = ithElement;
		for (unsigned ithAdjVtx = 0; ithAdjVtx < adjID.length(); ++ithAdjVtx)
		{
			//dataArray[ithElement]  = adjID[ithAdjVtx]+1; // matlab indices starts from 1
			dataArray [ithElement] = 1;
			rowIDArray[ithElement] = adjID[ithAdjVtx]; 
			ithElement++;
		}
	}
	colRangeArray[nVertices] = nEdges*2;

	// create matlab matrix obj
	MFnPluginData valFn;
	MObject valObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	pMat->getMatrix().setSparseDataPtr(dataArray, rowIDArray, colRangeArray, nVertices, nVertices,nEdges*2, nVertices+1, MatrixData::DT_UINT32);

	// set object
	MPlug vertMatPlug = getPlug(m_adjacencyMatrixName[0]);
	vertMatPlug.setValue(valObj);
	return s;
}

MStatus MeshToMatrixNode::computeVEIncidenceMatrix(MObject& meshObj)
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s);

	const int nVertices = meshFn.numVertices(&s);
	const int nEdges    = meshFn.numEdges(&s);
	if (nVertices <= 0 || nEdges <= 0)
		return MS::kFailure;

	// fill vertex array
	double*   dataArray = new double[nEdges*2];
	mwIndex   *   rowIDArray= new mwIndex[nEdges*2];
	mwIndex   *   colRangeArray = new mwIndex[nEdges+1];
	MItMeshEdge it(meshObj, &s);
	for (int ithEdge=0, ithElement = 0; !it.isDone(&s); it.next(),ithEdge++, ithElement += 2)
	{
		int vertID0 = it.index(0, &s);
		int vertID1 = it.index(1, &s);
		dataArray[ithElement]   = 1;
		dataArray[ithElement+1] = -1;
		rowIDArray[ithElement]  = vertID0;
		rowIDArray[ithElement+1]= vertID1;
		colRangeArray[ithEdge]  = ithEdge * 2;
	}
	colRangeArray[nEdges]  = nEdges * 2;

	// create matlab matrix obj
	MFnPluginData valFn;
	MObject valObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	pMat->getMatrix().setSparseDataPtr(dataArray, rowIDArray, colRangeArray, nVertices, nEdges, nEdges*2, nEdges+1, MatrixData::DT_DOUBLE);

	// set object
	MPlug vertMatPlug = getPlug(m_VEIncidenceMatrixName[0]);
	vertMatPlug.setValue(valObj);

	return s;
}


MStatus MeshToMatrixNode::computeVFIncidenceMatrix(MObject& meshObj)
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s);

	const int nVertices = meshFn.numVertices(&s);
	const int nFaces    = meshFn.numPolygons(&s);
	if (nVertices <= 0 || nFaces <= 0)
		return MS::kFailure;

	// fill vertex array
	std::vector<unsigned> dataArray;
	std::vector<mwIndex>  rowIDArray;
	mwIndex   *   colRangeArray = new mwIndex[nFaces+1];
	colRangeArray[0] = 0;
	mwIndex nNonZeroElements = 0;
	MItMeshPolygon it(meshObj, &s);
	for (int ithFace=0; !it.isDone(&s); it.next(),ithFace++)
	{
		MIntArray vIDs;
		it.getVertices(vIDs);
		for (unsigned ithV = 0; ithV < vIDs.length(); ++ithV, ++nNonZeroElements)
		{
			dataArray.push_back(ithV+1);
			rowIDArray.push_back(vIDs[ithV]);
		}
		colRangeArray[ithFace+1]  = nNonZeroElements;
	}

	// create matlab matrix obj
	MFnPluginData valFn;
	MObject valObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	unsigned* d   = new unsigned[nNonZeroElements];
	mwIndex   * rID   = new mwIndex[nNonZeroElements];
	memcpy(d,   &dataArray[0],  sizeof(unsigned)*nNonZeroElements);
	memcpy(rID, &rowIDArray[0], sizeof(mwIndex)*nNonZeroElements);
	pMat->getMatrix().setSparseDataPtr(	d,rID, colRangeArray, 
										nVertices, nFaces, nNonZeroElements, nFaces+1, MatrixData::DT_UINT32);

	// set object
	MPlug vertMatPlug = getPlug(m_VFIncidenceMatrixName[0]);
	vertMatPlug.setValue(valObj);

	return s;
}
void* MeshToMatrixNode::creator()
{
	return new MeshToMatrixNode;
}

MStatus MeshToMatrixNode::initialize()
{
	// This sample creates a single input float attribute and a single
	// output float attribute.
	//
	MStatus				stat;

	// add instruction attribute
	MFnTypedAttribute   tAttr;
	MFnEnumAttribute    eAttr;
	MFnMatrixAttribute  mAttr;
	m_inMesh = tAttr.create(m_inMeshName[0], m_inMeshName[1], MFnData::kMesh, &stat);
	tAttr.setStorable(false);
	tAttr.setHidden(false);
	tAttr.setWritable(true);
	tAttr.setReadable(true);
	tAttr.setArray(false);
	tAttr.setKeyable(true);

	m_vertexMatrix = tAttr.create(m_vertexMatrixName[0], m_vertexMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_normalMatrix = tAttr.create(m_normalMatrixName[0], m_normalMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_tangentMatrix = tAttr.create(m_tangentMatrixName[0], m_tangentMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_bitangentMatrix = tAttr.create(m_bitangentMatrixName[0], m_bitangentMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_VEIncidenceMatrix = tAttr.create(m_VEIncidenceMatrixName[0], m_VEIncidenceMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_VFIncidenceMatrix = tAttr.create(m_VFIncidenceMatrixName[0], m_VFIncidenceMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_cotLaplacianMatrix = tAttr.create(m_cotLaplacianMatrixName[0], m_cotLaplacianMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_voronoiArea = tAttr.create(m_voronoiAreaName[0], m_voronoiAreaName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_adjacencyMatrix = tAttr.create(m_adjacencyMatrixName[0], m_adjacencyMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_minCurDirMatrix = tAttr.create(m_minCurDirMatrixName[0], m_minCurDirMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);


	m_maxCurDirMatrix = tAttr.create(m_maxCurDirMatrixName[0], m_maxCurDirMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);


	m_minCurMatrix = tAttr.create(m_minCurMatrixName[0], m_minCurMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);


	m_maxCurMatrix = tAttr.create(m_maxCurMatrixName[0], m_maxCurMatrixName[1], MFnData::kPlugin);
	tAttr.setStorable(true);
	tAttr.setHidden(false);
	tAttr.setWritable(false);

	m_transformMatrix = mAttr.create(m_transformMatrixName[0], m_transformMatrixName[1]);
	mAttr.setHidden(false);
	mAttr.setReadable(false);
	mAttr.setWritable(true);
	mAttr.setKeyable(true);

	m_space        = eAttr.create(m_spaceName[0], m_spaceName[1], 0);
	eAttr.addField("Object Space", 0);
	eAttr.addField("World Space", 1);
	eAttr.setHidden(false);
	eAttr.setReadable(true);
	eAttr.setWritable(true);

	// Add the attributes we have created to the node
	//
	CHECK_STATUS(stat);
	stat = addAttribute( m_inMesh );
	CHECK_STATUS(stat);
	stat = addAttribute( m_vertexMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_normalMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_tangentMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_bitangentMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_VEIncidenceMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_VFIncidenceMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_adjacencyMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_minCurDirMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_maxCurDirMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_minCurMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_maxCurMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_space );
	CHECK_STATUS(stat);
	stat = addAttribute( m_transformMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_cotLaplacianMatrix );
	CHECK_STATUS(stat);
	stat = addAttribute( m_voronoiArea );
	CHECK_STATUS(stat);


	// Set up a dependency between the input and the output.  This will cause
	// the output to be marked dirty when the input changes.  The output will
	// then be recomputed the next time the value of the output is requested.
	//
	stat = attributeAffects( m_inMesh, m_vertexMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_normalMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_tangentMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_bitangentMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_VEIncidenceMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_VFIncidenceMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_adjacencyMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_minCurDirMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_maxCurDirMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_minCurMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_maxCurMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_cotLaplacianMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_inMesh, m_voronoiArea );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_space, m_vertexMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_space, m_normalMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_transformMatrix, m_vertexMatrix );
	CHECK_STATUS(stat);
	stat = attributeAffects( m_transformMatrix, m_normalMatrix );
	CHECK_STATUS(stat);

	return MS::kSuccess;
}

MPlug MeshToMatrixNode::getPlug( const char* longName )
{
	MObject nodeObj = thisMObject();
	MFnDependencyNode nodeFn(nodeObj);
	return nodeFn.findPlug(longName);
}

MStatus MeshToMatrixNode::computePrincipleCurvature( MObject& meshObj )
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s);

	const int nVertices = meshFn.numVertices(&s);
	const int nEdges    = meshFn.numEdges(&s);
	if (nVertices <= 0 || nEdges <= 0)
		return MS::kFailure;

	// create matlab matrix obj
	MFnPluginData dataFn;
	MObject minCurDirObj = dataFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMinCurDirData = (MatlabMatrix*)dataFn.data(&s);
	CHECK_STATUS(s);
	MObject maxCurDirObj = dataFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMaxCurDirData = (MatlabMatrix*)dataFn.data(&s);
	CHECK_STATUS(s);
	MObject minCurObj = dataFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMinCurData = (MatlabMatrix*)dataFn.data(&s);
	CHECK_STATUS(s);
	MObject maxCurObj = dataFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* pMaxCurData = (MatlabMatrix*)dataFn.data(&s);
	CHECK_STATUS(s);

	double* minCurDirArray = new double[nVertices * 3];
	double* maxCurDirArray = new double[nVertices * 3];
	double* minCurArray    = new double[nVertices];
	double* maxCurArray    = new double[nVertices];
	MItMeshVertex pVtx(meshObj, &s);
	for (int ithVtx = 0; !pVtx.isDone(&s); pVtx.next(), ++ithVtx)
	{
		MIntArray nearEdgeArray;
		pVtx.getConnectedEdges(nearEdgeArray);
		int numNearEdges = nearEdgeArray.length();
		MPoint vtxPos = pVtx.position();

		// build tensor matrix
		double tensor3x3[6] = {0.0};
		MItMeshEdge pPrevEdge(meshObj);
		MItMeshEdge pCurrEdge(meshObj);
		MItMeshEdge pNextEdge(meshObj);
		for (int i = 0; i < numNearEdges; i++)
		{

			// get edge iter
			int iPrev = (i+numNearEdges-1) % numNearEdges;
			int iNext = (i+numNearEdges+1) % numNearEdges;
			int tmp;
			pCurrEdge.setIndex(nearEdgeArray[i], tmp);
			pPrevEdge.setIndex(nearEdgeArray[iPrev], tmp);
			pNextEdge.setIndex(nearEdgeArray[iNext], tmp);

			int         nFaces;
			pCurrEdge.numConnectedFaces(nFaces);
			if (nFaces != 2)
				continue;

			// edge direction
			MVector		edgeDirection     = pCurrEdge.center() - vtxPos;
			MVector     prevEdgeDirection = pPrevEdge.center() - vtxPos;
			MVector     nextEdgeDirection = pNextEdge.center() - vtxPos;
			edgeDirection.normalize();
			prevEdgeDirection.normalize();
			nextEdgeDirection.normalize();

			// get face normal
			MVector normal0 = prevEdgeDirection ^ edgeDirection;
			MVector normal1 = edgeDirection ^ nextEdgeDirection;
			normal0.normalize();
			normal1.normalize();

			double      edgeLength;
			pCurrEdge.getLength(edgeLength);

			MVector     n0n1Cross  = normal0 ^ normal1;

			// compute angle
			//float sinAngle = -(float)(normal0 ^ normal1).length();// * edgeDirection;
			double sinAngle = n0n1Cross * edgeDirection;
			sinAngle = CLAMP_FLOAT(-1.f, 1.f, sinAngle);
			double angle = asin(sinAngle);

			// build tensor
			double coef  = angle * edgeLength;
			tensor3x3[0] += coef * edgeDirection.x * edgeDirection.x;
			tensor3x3[1] += coef * edgeDirection.x * edgeDirection.y;
			tensor3x3[2] += coef * edgeDirection.y * edgeDirection.y;
			tensor3x3[3] += coef * edgeDirection.x * edgeDirection.z;
			tensor3x3[4] += coef * edgeDirection.y * edgeDirection.z;
			tensor3x3[5] += coef * edgeDirection.z * edgeDirection.z; 

		}

		// extract eigenvalues and eigenvectors of tensor matrix
		// double eigen_values[3];  // will be given in decreasing order
		// double eigen_vectors[9]; // the same
		Matrix3d tensorMat;
		tensorMat <<	tensor3x3[0] , tensor3x3[1] , tensor3x3[3],
						tensor3x3[1] , tensor3x3[2] , tensor3x3[4],
						tensor3x3[3] , tensor3x3[4] , tensor3x3[5];
		Eigen::SelfAdjointEigenSolver<Matrix3d> eigensolver(tensorMat);
		if (eigensolver.info() != Success)
		{
			minCurDirArray[ithVtx]				= 0;
			minCurDirArray[ithVtx+nVertices]	= 0;
			minCurDirArray[ithVtx+nVertices*2]	= 0;
			maxCurDirArray[ithVtx]				= 0;
			maxCurDirArray[ithVtx+nVertices]	= 0;
			maxCurDirArray[ithVtx+nVertices*2]	= 0;
			maxCurArray[ithVtx]					= 0;
			minCurArray[ithVtx]					= 0;
			continue;
		}
		// find min/max eigen vector
		VectorXd eigValue  = eigensolver.eigenvalues();
		MatrixXd eigVector = eigensolver.eigenvectors();

		double absEig[3] = {abs(eigValue(0)), abs(eigValue(1)),abs(eigValue(2))};
		int    indices[3]; // {normal, minEig, maxEig}
		if(absEig[0] < absEig[1] && absEig[0] < absEig[2])
		{
			indices[0] = 0;
			indices[1] = eigValue(1) < eigValue(2) ? 1 : 2;
			indices[2] = eigValue(1) < eigValue(2) ? 2 : 1;
		}
		else // min_abs = 1
			if(absEig[1] < absEig[0] && absEig[1] < absEig[2])
			{
				indices[0] = 1;
				indices[1] = eigValue(0) < eigValue(2) ? 0 : 2;
				indices[2] = eigValue(0) < eigValue(2) ? 2 : 0;
			}
			else // min_abs = 2
			{
				indices[0] = 2;
				indices[1] = eigValue(0) < eigValue(1) ? 0 : 1;
				indices[2] = eigValue(0) < eigValue(1) ? 1 : 0;
			}

			// fill min/max dirmat
			int minCol = indices[1];
			int maxCol = indices[2];
			minCurDirArray[ithVtx]				= eigVector(0, minCol);
			minCurDirArray[ithVtx+nVertices]	= eigVector(1, minCol);
			minCurDirArray[ithVtx+nVertices*2]	= eigVector(2, minCol);
			maxCurDirArray[ithVtx]				= eigVector(0, maxCol);
			maxCurDirArray[ithVtx+nVertices]	= eigVector(1, maxCol);
			maxCurDirArray[ithVtx+nVertices*2]	= eigVector(2, maxCol);

			minCurArray[ithVtx]					= eigValue(minCol);
			maxCurArray[ithVtx]					= eigValue(maxCol);
	}

	// set object
	pMinCurDirData->getMatrix().setDenseDataPtr(minCurDirArray, nVertices, 3, MatrixData::DT_DOUBLE);
	pMaxCurDirData->getMatrix().setDenseDataPtr(maxCurDirArray, nVertices, 3, MatrixData::DT_DOUBLE);
	pMinCurData->getMatrix().setDenseDataPtr(minCurArray, nVertices, 1, MatrixData::DT_DOUBLE);
	pMaxCurData->getMatrix().setDenseDataPtr(maxCurArray, nVertices, 1, MatrixData::DT_DOUBLE);
	MPlug minCurDirPlug = Global::getPlug(this, m_minCurDirMatrixName[0]);
	MPlug maxCurDirPlug = Global::getPlug(this, m_maxCurDirMatrixName[0]);
	MPlug minCurPlug    = Global::getPlug(this, m_minCurMatrixName[0]);
	MPlug maxCurPlug    = Global::getPlug(this, m_maxCurMatrixName[0]);

	minCurDirPlug.setValue(minCurDirObj);
	maxCurDirPlug.setValue(maxCurDirObj);
	minCurPlug.setValue(minCurObj);
	maxCurPlug.setValue(maxCurObj);
	return s;
}

MMatrix MeshToMatrixNode::getTransMat()
{
	MPlug transMatPlug = Global::getPlug(this, m_transformMatrixName[0]);
	MObject transMatObject = transMatPlug.asMObject();
	MFnMatrixData transMatData(transMatObject);
	return transMatData.matrix();
}

MStatus MeshToMatrixNode::computeLocalFrameMatrix( MObject& meshObj )
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s);

	const int nVertices = meshFn.numVertices(&s);
	if (nVertices <= 0)
		return MS::kFailure;
	
	// get transform matrix
	MPlug transMatPlug = Global::getPlug(this, m_transformMatrixName[0]);
	MObject transMatObject = transMatPlug.asMObject();
	MFnMatrixData transMatData(transMatObject);
	MMatrix transMat = transMatData.matrix();
	MSpace::Space space = getCurrentSpace();
	if (space == MSpace::kObject)
		transMat.setToIdentity();

	// fill vertex array
	double* norArray      = new double[nVertices*3];
	double* tanArray      = new double[nVertices*3];
	double* bitanArray    = new double[nVertices*3];
	MItMeshVertex it(meshObj, &s);
	for (int ithVert=0; !it.isDone(&s); it.next(),ithVert++)
	{
		MVectorArray normals;
		it.getNormals(normals, space);

		MVector n(0,0,0), d(1,0,0);
		for (unsigned ithNor = 0; ithNor < normals.length();++ithNor)
		{
			n += normals[ithNor].transformAsNormal(transMat);
		}
		n.normalize();

		// compute tangent and bitangent
		MIntArray    neighIdx;
		it.getConnectedVertices(neighIdx);
		if (neighIdx.length())
		{
			MPoint thisPnt, neiPnt;
			meshFn.getPoint(neighIdx[0], neiPnt, space);
			thisPnt = it.position(space);
			d = MVector(neiPnt) - MVector(thisPnt);
		}
		MVector bitan = n ^ d;
		bitan.normalize();
		MVector tan   = bitan ^ n;
		tan.normalize();

		norArray[ithVert] = n.x;
		norArray[ithVert + nVertices] = n.y;
		norArray[ithVert + nVertices + nVertices] = n.z;

		tanArray[ithVert] = tan.x;
		tanArray[ithVert + nVertices] = tan.y;
		tanArray[ithVert + nVertices + nVertices] = tan.z;

		bitanArray[ithVert] = bitan.x;
		bitanArray[ithVert + nVertices] = bitan.y;
		bitanArray[ithVert + nVertices + nVertices] = bitan.z;
	}

	// create matlab matrix obj
	MFnPluginData valFn;
	MObject nObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* nMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	nMat->getMatrix().setDenseDataPtr(norArray, nVertices, 3, MatrixData::DT_DOUBLE);

	MObject tObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* tMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	tMat->getMatrix().setDenseDataPtr(tanArray, nVertices, 3, MatrixData::DT_DOUBLE);

	MObject btObj = valFn.create(MatlabMatrix::getTypeId(), &s);
	CHECK_STATUS(s);
	MatlabMatrix* btMat = (MatlabMatrix*)valFn.data(&s);
	CHECK_STATUS(s);
	btMat->getMatrix().setDenseDataPtr(bitanArray, nVertices, 3, MatrixData::DT_DOUBLE);

	// set object
	MPlug norMatPlug = getPlug(m_normalMatrixName[0]);
	norMatPlug.setValue(nObj);
	MPlug tanMatPlug = getPlug(m_tangentMatrixName[0]);
	tanMatPlug.setValue(tObj);
	MPlug bitanMatPlug = getPlug(m_bitangentMatrixName[0]);
	bitanMatPlug.setValue(btObj);

	return s;
}

MSpace::Space MeshToMatrixNode::getCurrentSpace()
{
	MPlug spacePlug = Global::getPlug(this, m_spaceName[0]);
	short idx = spacePlug.asShort();

	switch(idx)
	{
	case 0: return MSpace::kObject;
	case 1: return MSpace::kWorld;
	default:return MSpace::kObject;
	}
}

MStatus MeshToMatrixNode::computeCotLaplacian( MObject& meshObj)
{
	MStatus s;
	MFnMesh meshFn(meshObj, &s);
	CHECK_STATUS(s);

	MMatrix transMat = getTransMat();
	MSpace::Space space = getCurrentSpace();
	if (space == MSpace::kObject)
		transMat.setToIdentity();

	const int nVertices = meshFn.numVertices(&s);
	const int nEdges    = meshFn.numEdges(&s);
	if (nVertices <= 0 || nEdges <= 0)
		return MS::kFailure;

	// create matlab matrix obj
	MObject matObj;
	MatlabMatrix* matPtr;
	if (MS::kSuccess != MatlabMatrix::createMatrixObject(matObj, matPtr))
	{
		return MS::kFailure;
	}
	MObject areaObj;
	MatlabMatrix* areaPtr;
	if (MS::kSuccess != MatlabMatrix::createMatrixObject(areaObj, areaPtr))
	{
		return MS::kFailure;
	}

	// 分配数据
	int nElements = nVertices + nEdges * 2;
	double * dataBuffer = new double[nElements];
	mwIndex * rowIDBuffer= new mwIndex[nElements];
	mwIndex * rowRangeBuffer = new mwIndex[nVertices+1];
	double* pData = dataBuffer;
	mwIndex   * pRowID = rowIDBuffer;
	mwIndex   * pRowRange = rowRangeBuffer;
	*(pRowRange++) = 0;
	double* areaBuffer = new double[nVertices]; 

	// 构建拉普拉斯矩阵
	double* colMap = new double[nVertices];
	for (int i = 0; i < nVertices; ++i)
		colMap[i] = 0.0;

	struct NearPoint
	{
		int id;
		MVector pos;
		double cotPrev, cotNext;
	};
	vector<NearPoint>nearPoints;
	nearPoints.reserve(20);

	MItMeshVertex it(meshObj, &s);
	int nthVertex = 0;
	for (; !it.isDone(&s); it.next(),nthVertex++)
	{

		MPoint p = getMeshPoint(nthVertex, meshFn, transMat, space);
		const MVector centerPoint(p.x, p.y, p.z);

		// 记录邻点坐标
		MIntArray adjID;
		it.getConnectedVertices(adjID);
		nearPoints.clear();
		int nthNeighbour;
		for (nthNeighbour = 0; nthNeighbour < adjID.length(); ++nthNeighbour)
		{
			int nearVertID = adjID[nthNeighbour]; //vertexIDMap[itNear.handle()];
			MPoint nearPnt = getMeshPoint(nearVertID, meshFn, transMat, space);
			NearPoint nearPoint;
			nearPoint.id  = nearVertID;
			nearPoint.pos = MVector(nearPnt.x, nearPnt.y, nearPnt.z);
			nearPoints.push_back(nearPoint);
		}

		// 计算邻点正切值
		int nNeighbours = nthNeighbour;
		for (nthNeighbour = 0; nthNeighbour < nNeighbours; ++nthNeighbour)
		{
			int nextIdx = (nthNeighbour + 1) % nNeighbours;
			int prevIdx = (nthNeighbour + nNeighbours - 1) % nNeighbours;
			MVector& curPoint= nearPoints[nthNeighbour].pos;
			MVector nextDir = nearPoints[nextIdx].pos - curPoint;
			MVector prevDir = nearPoints[prevIdx].pos - curPoint;
			MVector centerDir= centerPoint - curPoint;
			nextDir.normalize();
			prevDir.normalize();
			centerDir.normalize();
			double cosNext = centerDir* nextDir;
			double cosPrev = centerDir* prevDir;
			nearPoints[nthNeighbour].cotPrev = tan(M_PI_2 - acos(cosPrev));
			nearPoints[nthNeighbour].cotNext = tan(M_PI_2 - acos(cosNext));
			double c = tan(M_PI_2 - acos(cosPrev));
			c = tan(M_PI_2 - acos(cosNext));
		}

		// 计算拉普拉斯矩阵值
		double totalCot = 0;
		double totalArea = 0;
		for (nthNeighbour = 0; nthNeighbour < nNeighbours; ++nthNeighbour)
		{
			int nextIdx = (nthNeighbour + 1) % nNeighbours;
			int prevIdx = (nthNeighbour + nNeighbours - 1) % nNeighbours;
			double cotValue = -0.5 * (nearPoints[nextIdx].cotPrev + nearPoints[prevIdx].cotNext);
			colMap[nearPoints[nthNeighbour].id] = cotValue;
			totalCot += cotValue;

			// 计算面积
			MVector& curPoint= nearPoints[nthNeighbour].pos;
			MVector& nextPoint = nearPoints[nextIdx].pos;
			MVector  curDir  = curPoint - centerPoint;
			MVector  nextDir = nextPoint - centerPoint;
			double curLength    = curDir.length();
			double nextLength   = nextDir.length();
			double cosAngle     = curDir * nextDir / (curLength * nextLength);
			double sinAngle     = sqrt(1 - cosAngle * cosAngle);
			double area;
			if (curDir * (curDir - nextDir) < 0 ||
				nextDir* (nextDir - curDir) < 0)
				area = curLength * nextLength * sinAngle * 0.25f; // 钝角三角形
			else if(cosAngle < 0.f)					
				area = curLength * nextLength * sinAngle * 0.5f; // 钝角三角形
			else
			{
				double chordLength  = (nextDir - curDir).length();
				double radius       = chordLength / sinAngle * 0.5;
				double halfCurLength = curLength  * 0.5;
				double halfNextLength= nextLength * 0.5;
				double curHeight    = sqrt(radius * radius - halfCurLength * halfCurLength);
				double nextHeight   = sqrt(radius * radius - halfNextLength * halfNextLength);
				area = 0.5 * (halfCurLength * curHeight + halfNextLength * nextHeight);
			}
			totalArea += area;

		}
		colMap[nthVertex] = -totalCot;
		areaBuffer[nthVertex] = totalArea;

		for (int i = 0; i < nVertices; ++i)
		{
			if (colMap[i] != 0.0)
			{
				*(pData++) = colMap[i];
				colMap[i] = 0.0;
				*(pRowID++) = i;
			}
		}
		*(pRowRange++) = pData - dataBuffer;
	}

	/*
	if (areaWeighted)
	{
		for (int ithElement = 0; ithElement < nElements; ++ithElement)
		{
			mwIndex ithRow = rowIDBuffer[ithElement];
			dataBuffer[ithElement] /= areaBuffer[ithRow];
		}
	}*/
	delete[] colMap;

	matPtr->getMatrix().setSparseDataPtr(
		dataBuffer, rowIDBuffer, rowRangeBuffer, 
		nVertices, nVertices, 
		nVertices + nEdges*2, nVertices+1, 
		MatrixData::DT_DOUBLE);

	areaPtr->getMatrix().setDenseDataPtr(areaBuffer, nVertices, 1, MatrixData::DT_DOUBLE);

	// set object
	MPlug lapMatPlug = getPlug(m_cotLaplacianMatrixName[0]);
	lapMatPlug.setValue(matObj);
	MPlug areaPlug   = getPlug(m_voronoiAreaName[0]);
	areaPlug.setValue(areaObj);

	return s;
}





