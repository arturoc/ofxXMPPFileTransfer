/*
 * ofxXMPPFileTransfer.h
 *
 *  Created on: Sep 20, 2013
 *      Author: arturo
 */

#ifndef OFXXMPPFILETRANSFER_H_
#define OFXXMPPFILETRANSFER_H_

#include "ofxNice.h"
#include "ofxXMPP.h"
#include "ofConstants.h"
#include "ofFileUtils.h"
#include "ofThread.h"
#include "Poco/Condition.h"
#include "Poco/SHA1Engine.h"

#include "Poco/UUIDGenerator.h"
#include "Poco/UUID.h"

struct ofxXMPPFileReceive{
	ofxXMPPFileReceive(ofxXMPPJingleFileInitiation fileInitiation)
	:writeMode(false)
	,metadata(fileInitiation)
	,inMemoryTransfer(true)
	,writeSize(0)
	,hashReceived(false)
	,progressPct(0){}

	ofxXMPPFileReceive(ofxXMPPJingleFileInitiation fileInitiation, ofDirectory & dir)
	:writeMode(false)
	,metadata(fileInitiation)
	,file(ofFilePath::join(dir.path(),fileInitiation.name),ofFile::Reference,true)
	,inMemoryTransfer(false)
	,writeSize(0)
	,hashReceived(false)
	,progressPct(0){}

	bool isComplete(){
		if(inMemoryTransfer){
			return data.size() == metadata.size;
		}else{
			return writeSize == metadata.size;
		}
	}

	void append(const ofBuffer & buffer){
		if(inMemoryTransfer){
			data.append(buffer.getBinaryBuffer(),buffer.size());
		}else{
			if(!writeMode){
				file.changeMode(ofFile::WriteOnly,true);
				writeMode = true;
			}
			file.writeFromBuffer(buffer);
			writeSize+=buffer.size();
		}
	}

	bool checkHash(string hash){
		return hashReceived && remoteHash.hash==hash;
	}

	const ofxXMPPJingleFileInitiation & getMetadata() const{
		return metadata;
	}

	const ofBuffer & getBuffer(){
		return data;
	}

	void setRemoteHash(const ofxXMPPJingleHash & hash){
		remoteHash = hash;
		hashReceived = true;
	}

	ofxXMPPJingleHash getRemoteHash(){
		return remoteHash;
	}


	bool isMemoryTransfer(){
		return inMemoryTransfer;
	}

	bool hasRemoteHash(){
		return hashReceived;
	}

	string path(){
		return file.path();
	}

private:
	bool writeMode;
	ofxXMPPJingleFileInitiation metadata;
	ofBuffer data;
	ofFile file;
	bool inMemoryTransfer;
	size_t writeSize;
	ofxXMPPJingleHash remoteHash;
	bool hashReceived;
	float progressPct;
};

struct ofxXMPPFileSend{
	ofxXMPPFileSend()
	:progressPct(0){
		metadata.fid = Poco::UUIDGenerator().create().toString();
		cout << "created file to send with fid=" << metadata.fid << endl;
	}

	ofxXMPPJingleFileInitiation metadata;
	ofFile file;
	float progressPct;
};

class ofxXMPPFileTransferSession: public ofThread{
public:
	ofxXMPPFileTransferSession(ofxXMPP * xmpp);

	void initControlling(const string & to);
	bool sendFile(const string & path);

	void acceptInMemoryFileTransfer(ofxXMPPJingleFileInitiation & fileInitiation);
	void acceptToFolderTransfer(ofxXMPPJingleFileInitiation & fileInitiation, ofDirectory & dir);

	ofEvent<ofxXMPPFileReceive> fileReceivedCorrectly;
	ofEvent<ofxXMPPFileReceive> fileSavedCorrectly;

private:
	void initSlave();
	void threadedFunction();
	void setupStream();

	void onFileInitiationReceived(ofxXMPPJingleFileInitiation & fileInitiation);
	void onFileInitiationAccepted(ofxXMPPJingleFileInitiation & fileInitiation);
	void onLocalCandidatesGathered(vector<ofxICECandidate> & candidates);
	void onDataReceived(ofBuffer & buffer);
	void onReliableWritable(int & componentId);
	void onHashReceived(ofxXMPPJingleHash & hash);
	void onComponentReady(int & componentId);
	void onHashACKd(ofxXMPPJingleHash & hash);

	Poco::Condition condition;

	string remoteUser;
	queue<ofxXMPPFileSend> filesToSend;
	deque<ofxXMPPFileSend> filesToSendNotInitiated;
	deque<ofxXMPPFileSend> filesToSendNotAccepted;

	queue<ofxXMPPFileReceive> filesToReceive;
	queue<ofxXMPPFileReceive> filesToReceiveNotAccepted;

	enum State{
		Disconnected,
		StartingSession,
		GotLocalCandidates,
		SessionStablished
	}state;


	ofxNiceAgent niceAgent;
	ofxNiceStream niceStream;
	ofxXMPPICETransport localTransport;
	ofxXMPPICETransport remoteTransport;
	streamsize sizeSent;

	Poco::SHA1Engine sha1Send, sha1Receive;
	string localReceivedHash;

	bool controlling;
	int KBps;
	Poco::UUID sid;

	ofxXMPP * xmpp;

	ofMutex receiveMutex;
	bool lastFileSentACKd;

	friend class ofxXMPPFileTransfer;
};

class ofxXMPPFileTransfer{
public:
	ofxXMPPFileTransfer();
	virtual ~ofxXMPPFileTransfer();

	void setUnlimitedRate();
	void setTransferRate(int KBps);
	void setup(ofxXMPP & xmpp);


	bool sendFile(const string & to, const string & path);

	/// will trigger a fileReceivedCorrectly event when the transfer is done
	/// for big files it can use too much memory so better to accept to folder
	void acceptInMemoryFileTransfer(ofxXMPPJingleFileInitiation & fileInitiation);

	/// will directly save the file to the specified folder
	/// when it's done it'll trigger the fileSaved event
	void acceptToFolderTransfer(ofxXMPPJingleFileInitiation & fileInitiation, const string & dir);

	ofEvent<ofxXMPPJingleFileInitiation> fileTransferReceived;
	ofEvent<ofxXMPPFileReceive> fileReceivedCorrectly;
	ofEvent<ofxXMPPFileReceive> fileSavedCorrectly;

private:

	ofPtr<ofxXMPPFileTransferSession> getSession(string to);
	void onFileInitiationReceived(ofxXMPPJingleFileInitiation & fileInitiation);
	void onFileInitiationAccepted(ofxXMPPJingleFileInitiation & fileInitiation);
	void onHashReceived(ofxXMPPJingleHash & hash);

	void onFileReceivedCorrectly(ofxXMPPFileReceive & file);
	void onFileSavedCorrectly(ofxXMPPFileReceive & file);


	ofxXMPP * xmpp;

	map<string,ofPtr<ofxXMPPFileTransferSession> > sessions;
};

#endif /* OFXXMPPFILETRANSFER_H_ */
