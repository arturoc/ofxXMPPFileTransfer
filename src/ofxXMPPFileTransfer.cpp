/*
 * ofxXMPPFileTransfer.cpp
 *
 *  Created on: Sep 20, 2013
 *      Author: arturo
 */

#include "ofxXMPPFileTransfer.h"
#include "ofGstUtils.h"
#include "ofAppRunner.h"

ofxXMPPFileTransferSession::ofxXMPPFileTransferSession(ofxXMPP * xmpp)
:state(Disconnected)
,sizeSent(0)
,controlling(false)
,KBps(-1)
,sid(Poco::UUIDGenerator().create())
,xmpp(xmpp)
,lastFileSentACKd(true)
{
	startThread();
}

void ofxXMPPFileTransferSession::initControlling(const string & to){
	if(state == Disconnected){
		ofGstUtils::startGstMainLoop();
		niceAgent.setup("77.72.174.165",3478,true,ofGstUtils::getGstMainLoop(),NICE_COMPATIBILITY_RFC5245,true);
		remoteUser = to;
		state = StartingSession;
		controlling = true;

		ofAddListener(niceStream.localCandidatesGathered,this,&ofxXMPPFileTransferSession::onLocalCandidatesGathered);
		ofAddListener(niceStream.dataReceived,this,&ofxXMPPFileTransferSession::onDataReceived);
		ofAddListener(niceStream.reliableComponentWritable,this,&ofxXMPPFileTransferSession::onReliableWritable);
		ofAddListener(niceStream.componentReady,this,&ofxXMPPFileTransferSession::onComponentReady);
		ofAddListener(xmpp->hashACKd,this,&ofxXMPPFileTransferSession::onHashACKd);

		setupStream();
	}
}

void ofxXMPPFileTransferSession::initSlave(){
	if(state == Disconnected){
		ofGstUtils::startGstMainLoop();
		niceAgent.setup("77.72.174.165",3478,false,ofGstUtils::getGstMainLoop(),NICE_COMPATIBILITY_RFC5245,true);
		state = StartingSession;
		controlling = false;

		ofAddListener(niceStream.localCandidatesGathered,this,&ofxXMPPFileTransferSession::onLocalCandidatesGathered);
		ofAddListener(niceStream.dataReceived,this,&ofxXMPPFileTransferSession::onDataReceived);
		ofAddListener(niceStream.reliableComponentWritable,this,&ofxXMPPFileTransferSession::onReliableWritable);
		ofAddListener(niceStream.componentReady,this,&ofxXMPPFileTransferSession::onComponentReady);
		ofAddListener(xmpp->hashACKd,this,&ofxXMPPFileTransferSession::onHashACKd);

		setupStream();
	}
}

void ofxXMPPFileTransferSession::setupStream(){
	niceStream.setup(niceAgent,1);
	niceStream.listen();
	niceAgent.addStream(&niceStream);
	niceStream.gatherLocalCandidates();
}


bool ofxXMPPFileTransferSession::sendFile(const string & path){
	ofxXMPPFileSend send;
	send.file.open(path,ofFile::ReadOnly,true);
	if(send.file.exists()){
		send.metadata.name = ofFilePath::getFileName(path);
		send.metadata.sid = sid.toString();
		send.metadata.date=""; // TODO: add date
		send.metadata.desc=""; // TODO: add description
		send.metadata.size = send.file.getSize();

		if(state==GotLocalCandidates || state==SessionStablished){
			send.metadata.transport = localTransport;
			xmpp->initiateFileTransfer(remoteUser,send.metadata);

			lock();
			filesToSendNotAccepted.push_back(send);
			unlock();
		}else{
			lock();
			filesToSendNotInitiated.push_back(send);
			unlock();
		}


		return true;
	}else{
		ofLogError() << "file " << path << " doesn't exist";
		return false;
	}
}


void ofxXMPPFileTransferSession::acceptInMemoryFileTransfer(ofxXMPPJingleFileInitiation & fileInitiation){
	initSlave();

	ofxXMPPFileReceive receive(fileInitiation);

	if(state == GotLocalCandidates || state == SessionStablished){
		ofxXMPPJingleFileInitiation localFileInitiation = fileInitiation;
		localFileInitiation.transport = localTransport;
		xmpp->acceptFileTransfer(localFileInitiation);
		receiveMutex.lock();
		filesToReceive.push(receive);
		receiveMutex.unlock();
	}else{
		filesToReceiveNotAccepted.push(receive);
	}

}

void ofxXMPPFileTransferSession::acceptToFolderTransfer(ofxXMPPJingleFileInitiation & fileInitiation, ofDirectory & dir){
	initSlave();

	ofxXMPPFileReceive receive(fileInitiation,dir);

	if(state == GotLocalCandidates || state == SessionStablished){
		ofxXMPPJingleFileInitiation localFileInitiation = fileInitiation;
		localFileInitiation.transport = localTransport;
		xmpp->acceptFileTransfer(localFileInitiation);
		receiveMutex.lock();
		filesToReceive.push(receive);
		receiveMutex.unlock();
	}else{
		filesToReceiveNotAccepted.push(receive);
	}
}

void ofxXMPPFileTransferSession::onFileInitiationReceived(ofxXMPPJingleFileInitiation & fileInitiation){
	if(state==Disconnected){
		remoteTransport = fileInitiation.transport;
		remoteUser = fileInitiation.from;
	}
}

void ofxXMPPFileTransferSession::onFileInitiationAccepted(ofxXMPPJingleFileInitiation & fileInitiation){
	xmpp->ack(fileInitiation);

	if(state == StartingSession || state == GotLocalCandidates){
		niceStream.setRemoteCredentials(fileInitiation.transport.ufrag,fileInitiation.transport.pwd);
		niceStream.setRemoteCandidates(fileInitiation.transport.candidates);
		remoteTransport = fileInitiation.transport;
	}

	lock();
	for(size_t i=0;i<filesToSendNotAccepted.size();i++){
		if(filesToSendNotAccepted[i].metadata.fid == fileInitiation.fid){
			filesToSend.push(filesToSendNotAccepted[i]);
			filesToSendNotAccepted.erase(filesToSendNotAccepted.begin()+i);
			condition.signal();
			break;
		}
	}
	unlock();
}

void ofxXMPPFileTransferSession::onComponentReady(int & componentId){
	state = SessionStablished;
}

void ofxXMPPFileTransferSession::onReliableWritable(int & componentId){
	lock();
	condition.signal();
	unlock();
}

void ofxXMPPFileTransferSession::onHashACKd(ofxXMPPJingleHash & hash){
	lock();
	lastFileSentACKd = true;
	filesToSend.pop();
	condition.signal();
	unlock();
}

void ofxXMPPFileTransferSession::onLocalCandidatesGathered(vector<ofxICECandidate> & candidates){
	localTransport.ufrag = niceStream.getLocalUFrag();
	localTransport.pwd = niceStream.getLocalPwd();
	localTransport.candidates = candidates;


	if(!controlling && state==StartingSession){
		niceStream.setRemoteCredentials(remoteTransport.ufrag,remoteTransport.pwd);
		niceStream.setRemoteCandidates(remoteTransport.candidates);

		while(!filesToReceiveNotAccepted.empty()){
			ofxXMPPJingleFileInitiation localFileInitiation = filesToReceiveNotAccepted.front().getMetadata();
			localFileInitiation.transport = localTransport;
			xmpp->acceptFileTransfer(localFileInitiation);
			receiveMutex.lock();
			filesToReceive.push(filesToReceiveNotAccepted.front());
			receiveMutex.unlock();
			filesToReceiveNotAccepted.pop();
		}
	}else if(controlling){
		while(!filesToSendNotInitiated.empty()){
			ofxXMPPJingleFileInitiation localFileInitiation = filesToSendNotInitiated.front().metadata;
			localFileInitiation.transport = localTransport;
			xmpp->initiateFileTransfer(remoteUser,localFileInitiation);

			lock();
			filesToSendNotAccepted.push_back(filesToSendNotInitiated.front());
			filesToSendNotInitiated.pop_front();
			unlock();
		}
	}

	state = GotLocalCandidates;

}

void ofxXMPPFileTransferSession::onHashReceived(ofxXMPPJingleHash & hash){
	Poco::ScopedLock<ofMutex> lock(receiveMutex);
	if(filesToReceive.empty()){
		ofLogError() << "received hash but no files in queue";
		return;
	}

	cout << "hash received" << endl;
	filesToReceive.front().setRemoteHash(hash);

	if(filesToReceive.front().isComplete() && filesToReceive.front().checkHash(localReceivedHash)){
		cout << "file complete and hash correct notifying" << endl;
		if(filesToReceive.front().isMemoryTransfer())
			ofNotifyEvent(fileReceivedCorrectly,filesToReceive.front(),this);
		else
			ofNotifyEvent(fileSavedCorrectly,filesToReceive.front(),this);
		xmpp->ack(filesToReceive.front().getRemoteHash());
		filesToReceive.pop();
	}else if(filesToReceive.front().isComplete()){
		cout << "hash not correct" << endl;
	}
}

void ofxXMPPFileTransferSession::onDataReceived(ofBuffer & buffer){
	Poco::ScopedLock<ofMutex> lock(receiveMutex);
	if(filesToReceive.empty()){
		ofLogError() << "received data but no files in queue";
		return;
	}

	sha1Receive.update(buffer.getBinaryBuffer(),buffer.size());
	filesToReceive.front().append(buffer);

	if(filesToReceive.front().isComplete()){
		localReceivedHash = Poco::SHA1Engine::digestToHex(sha1Receive.digest());
		sha1Receive.reset();
	}

	if(filesToReceive.front().isComplete() && filesToReceive.front().hasRemoteHash() && filesToReceive.front().checkHash(localReceivedHash)){
		if(filesToReceive.front().isMemoryTransfer())
			ofNotifyEvent(fileReceivedCorrectly,filesToReceive.front(),this);
		else
			ofNotifyEvent(fileSavedCorrectly,filesToReceive.front(),this);
		xmpp->ack(filesToReceive.front().getRemoteHash());
		filesToReceive.pop();
	}else if(filesToReceive.front().isComplete() && filesToReceive.front().hasRemoteHash()){
		cout << "hash not correct" << endl;
	}
}

void ofxXMPPFileTransferSession::threadedFunction(){
	char buffer[1024];
	while(isThreadRunning()){
		lock();
		while(!lastFileSentACKd || filesToSend.empty() || state!=SessionStablished){
			cout << "waiting on " << filesToSend.size() <<  " files to send and state = " << state << endl;
			condition.wait(mutex);
		}
		ofFile fileToRead = filesToSend.front().file;
		ofxXMPPJingleFileInitiation nextJingle = filesToSend.front().metadata;
		unlock();

		cout << "sending file " << nextJingle.name << endl;
		sizeSent = 0;
		lastFileSentACKd = false;
		sha1Send.reset();
		while(fileToRead.good()){
			unsigned long long now = ofGetElapsedTimeMillis();
			fileToRead.read(buffer,1024);
			streamsize sizeRead = fileToRead.gcount();
			sizeSent += sizeRead;

			sha1Send.update(buffer,sizeRead);

			int err = 0;
			int pos = 0;
			while(pos<sizeRead){
				err = niceStream.sendRawData(buffer+pos,sizeRead-pos);
				if(err==-1){
					lock();
					condition.wait(mutex);
					unlock();
				}else{
					pos += err;
				}
			}

			if(KBps!=-1){
				unsigned long long timeDiff = ofGetElapsedTimeMillis() - now;
				unsigned long long frameTime = 1000./double(KBps);
				if(timeDiff<frameTime){
					ofSleepMillis(frameTime-timeDiff);
				}
			}
		}

		cout << "file sent" << endl;
		fileToRead.close();
		ofxXMPPJingleHash jingleHash;
		jingleHash.from = nextJingle.from;
		jingleHash.sid = nextJingle.sid;
		jingleHash.algo = "sha-1";
		jingleHash.hash = Poco::SHA1Engine::digestToHex(sha1Send.digest());
		xmpp->sendFileHash(remoteUser, jingleHash);
	}
}

/*TODO: this is by file sent/received
float ofxXMPPFileTransferSession::getProgress(){
	if(controlling){
		return double(sizeSent)/double(localFileInitiation.size);
	}else{
		if(inMemoryTransfer){
			return double(receivingFile.data.size()) / double(localFileInitiation.size);
		}else{
			return double(writeSize)/double(localFileInitiation.size);
		}
	}
}*/

ofxXMPPFileTransfer::ofxXMPPFileTransfer()
:xmpp(NULL){

}

ofxXMPPFileTransfer::~ofxXMPPFileTransfer() {
}


void ofxXMPPFileTransfer::setup(ofxXMPP & xmpp){
	this->xmpp = &xmpp;
	ofAddListener(xmpp.jingleFileInitiationReceived,this,&ofxXMPPFileTransfer::onFileInitiationReceived);
	ofAddListener(xmpp.jingleFileInitiationAccepted,this,&ofxXMPPFileTransfer::onFileInitiationAccepted);
	ofAddListener(xmpp.hashReceived,this,&ofxXMPPFileTransfer::onHashReceived);
}

ofPtr<ofxXMPPFileTransferSession> ofxXMPPFileTransfer::getSession(string to){
	ofPtr<ofxXMPPFileTransferSession> & session = sessions[to];
	if(!session){
		sessions[to] = ofPtr<ofxXMPPFileTransferSession>(new ofxXMPPFileTransferSession(xmpp));

		ofAddListener(sessions[to]->fileReceivedCorrectly,this,&ofxXMPPFileTransfer::onFileReceivedCorrectly);
		ofAddListener(sessions[to]->fileSavedCorrectly,this,&ofxXMPPFileTransfer::onFileSavedCorrectly);
	}
	return session;
}

bool ofxXMPPFileTransfer::sendFile(const string & to, const string & path){
	if(!sessions[to])
		getSession(to)->initControlling(to);
	return getSession(to)->sendFile(path);
}

void ofxXMPPFileTransfer::acceptInMemoryFileTransfer(ofxXMPPJingleFileInitiation & fileInitiation){
	getSession(fileInitiation.from)->acceptInMemoryFileTransfer(fileInitiation);
}

void ofxXMPPFileTransfer::acceptToFolderTransfer(ofxXMPPJingleFileInitiation & fileInitiation, const string & dir){
	ofDirectory folder(dir);
	folder.create(true);
	getSession(fileInitiation.from)->acceptToFolderTransfer(fileInitiation,folder);
}

void ofxXMPPFileTransfer::onFileInitiationReceived(ofxXMPPJingleFileInitiation & fileInitiation){
	getSession(fileInitiation.from)->onFileInitiationReceived(fileInitiation);
	xmpp->ack(fileInitiation);
	ofNotifyEvent(fileTransferReceived,fileInitiation,this);
}

void ofxXMPPFileTransfer::onFileInitiationAccepted(ofxXMPPJingleFileInitiation & fileInitiation){
	getSession(fileInitiation.from)->onFileInitiationAccepted(fileInitiation);
}

void ofxXMPPFileTransfer::onHashReceived(ofxXMPPJingleHash & hash){
	getSession(hash.from)->onHashReceived(hash);
}

void ofxXMPPFileTransfer::onFileReceivedCorrectly(ofxXMPPFileReceive & file){
	ofNotifyEvent(fileReceivedCorrectly,file,this);
}

void ofxXMPPFileTransfer::onFileSavedCorrectly(ofxXMPPFileReceive & file){
	ofNotifyEvent(fileSavedCorrectly,file,this);
}

void ofxXMPPFileTransfer::setUnlimitedRate(){
	//TODO: per session? KBps = -1;
}

void ofxXMPPFileTransfer::setTransferRate(int KBps){
	//TODO: per session? this->KBps = KBps;
}
