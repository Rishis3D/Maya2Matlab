#pragma once
/***********************************************************************
   MEL command: matlabEngine   
   -open			-o	open matlab engine
   -close			-c	close matlab engine
   -execCmd			-ec	execute a matlab command
   -execNode		-e  execute a selected or specified(use -name flag) matlab node
   -name			-n	specify a node's name when execute a node
   -closeAfterExec	-ca	close engine after execute a node
/************************************************************************/


class MatlabEngineCmd:public MPxCommand
{
public:
	MatlabEngineCmd(void);
	~MatlabEngineCmd(void);

	MStatus   	doIt( const MArgList& args );

	static void* creator();
	static MSyntax newSyntax();
private: 
	MStatus		execNode(MFnDependencyNode& nodeFn);
	static const char* m_execNodeFlag[2];					// ִ��ѡ�нڵ������
	static const char* m_closeAfterExecFlag[2];				// ��ִ�����֮��ر�����,���execNodeFlagʹ��
	static const char* m_nodeNameFlag[2];					// ָ��ĳ�����ֵĽڵ�ִ��,���execNodeFlagʹ��
	static const char* m_openEngineFlag[2];					// ������
	static const char* m_closeEngineFlag[2];				// �ر�����
	static const char* m_execCmdFlag[2];					// ִ���ַ�������
	static const int   m_cmdResultLength;					// cmdResult�ĳ���
	static char        m_cmdResult[];						// ����ִ�н�����ַ���		
};
