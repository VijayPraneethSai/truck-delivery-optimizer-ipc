#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>



typedef struct PackageRequest {
 int packageId;
 int pickup_x;
 int pickup_y;
 int dropoff_x;
 int dropoff_y;
 int arrival_turn;
 int expiry_turn;
} PackageRequest;

typedef struct MainSharedMemory {
 char authStrings[250][21];
 char truckMovementInstructions[250];
 int pickUpCommands[250];
 int dropOffCommands[250];

 int truckPositions[250][2];
 int truckPackageCount[250];
 int truckTurnsInToll[250];

 PackageRequest newPackageRequests[50];
 int packageLocations[5000][2];
} MainSharedMemory;

typedef struct TurnChangeResponse {
 long mtype;
 int turnNumber;
 int newPackageRequestCount;
 int errorOccured;
 int finished;
} TurnChangeResponse;

typedef struct TurnReadyRequest {
 long mtype;
} TurnReadyRequest;

typedef struct SolverRequest {
 long mtype;
 int truckNumber;
 char authStringGuess[21];
} SolverRequest;

typedef struct SolverResponse {
 long mtype; 
 int guessIsCorrect;
} SolverResponse;


int N, D, S, T, B;
key_t shmemKey, mainMesQkey;
key_t solverMesQkeys[250];
int shmemId;
MainSharedMemory *mainShmemPtr;
int mainMesQid;
int solverMesQids[250];


typedef struct {
 PackageRequest rqst;
 int pckUp; // The stauts of packge
 int trAsgnd; // If truk is asisnged
} PckgSt;

PckgSt pckgs[5000];
int totPckgs = 0;






void authStrFrSolver(int trId, int pckgCt, char *oStr) {
 if (pckgCt == 0) {
 strcpy(oStr, "");
 return;
 }

 int slvIdx = trId % S;
 int slvQ = solverMesQids[slvIdx];

 
 SolverRequest stTr = {0}; 
 stTr.mtype = 2;
 stTr.truckNumber = trId;

 int sndMsgSizeTr = sizeof(stTr) - sizeof(long);
 if (msgsnd(slvQ, &stTr, sndMsgSizeTr, 0) == -1){ //steting truck numebr
 perror("Error in sending message");
 return;
 }
 
 char gs[21];
 const char moves[] = {'u', 'd', 'l', 'r'};

 while (1) { //guessing the stirng
 for (int i = 0; i < pckgCt; i++){ // creating random stirng
 gs[i] = moves[rand() % 4];
 }
 gs[pckgCt] = '\0';

 SolverRequest req = {0};
 req.mtype = 3;
 req.truckNumber = trId;
 strcpy(req.authStringGuess, gs);
 int sndMsgSizeGes = sizeof(req) - sizeof(long);
 if (msgsnd(slvQ, &req, sndMsgSizeGes, 0) == -1){
 perror("Error in sending message");
 return ;
 }
 

 SolverResponse resp;
 int rcvMsgSizeRes = sizeof(resp) - sizeof(long);
 if (msgrcv(slvQ, &resp,rcvMsgSizeRes , 4, 0) == -1){
 perror("Error in sending message");
 return ;
 }
 

 if (resp.guessIsCorrect) {
 strcpy(oStr, gs); 
 return;
 }
 }
}




int main() {

 FILE *inpfile = fopen("input.txt", "r");
 if (!inpfile){
 perror("Error in opening input file");
 return 1;
 }

 fscanf(inpfile, "%d %d %d %d %d", &N, &D, &S, &T, &B);
 fscanf(inpfile, "%d", &shmemKey);
 fscanf(inpfile, "%d", &mainMesQkey);
 for (int i = 0; i < S; i++){
 fscanf(inpfile, "%d", &solverMesQkeys[i]);
 }
 fclose(inpfile);


 shmemId = shmget(shmemKey, sizeof(MainSharedMemory), 0666);
 if (shmemId == -1){
 perror("Error with shared memory");
 return 1;
 }
 mainShmemPtr = (MainSharedMemory *)shmat(shmemId, NULL, 0);
 if (mainShmemPtr == (void *)-1){
 perror("Error in shared memory attach");
 return 1;
 }

 mainMesQid = msgget(mainMesQkey, 0666);
 if (mainMesQid == -1){
 perror("Error in main msgget");
 return 1;
 }

 for (int i = 0; i < S; i++) {
 solverMesQids[i] = msgget(solverMesQkeys[i], 0666);
 if (solverMesQids[i] == -1){
 perror("Error in msgget for solver");
 return 1;
 }
 }

 srand(time(NULL)); 


 while (1) {
 TurnChangeResponse resForTurn;
 int rcvMsgSize = sizeof(resForTurn) - sizeof(long);
 if(msgrcv(mainMesQid, &resForTurn, rcvMsgSize, 2, 0) == -1){ //getring the repsonse for turn
 perror("Error in receiving message");
 return 1;
 }

 if (resForTurn.errorOccured == 1) {
 break;
 }
 if (resForTurn.finished == 1) {
 break;
 }

 if (resForTurn.newPackageRequestCount > 0){
 for (int i = 0; i < resForTurn.newPackageRequestCount; i++) {
 pckgs[totPckgs].rqst = mainShmemPtr->newPackageRequests[i];
 pckgs[totPckgs].pckUp = 0; //packaeg is not picked
 pckgs[totPckgs].trAsgnd = -1; // packeg is not asisnged
 totPckgs++;
 }
 
 }
 

 for (int trno = 0; trno < D; trno++) {
 if (mainShmemPtr->truckTurnsInToll[trno] > 0) {
 mainShmemPtr->truckMovementInstructions[trno] = 's';
 mainShmemPtr->pickUpCommands[trno] = -1;
 mainShmemPtr->dropOffCommands[trno] = -1;
 authStrFrSolver(trno, mainShmemPtr->truckPackageCount[trno], mainShmemPtr->authStrings[trno]);
 continue;
 }

 int trx = mainShmemPtr->truckPositions[trno][0];
 int try = mainShmemPtr->truckPositions[trno][1];
 int pkgCount = mainShmemPtr->truckPackageCount[trno];

 mainShmemPtr->truckMovementInstructions[trno] = 's';
 mainShmemPtr->pickUpCommands[trno] = -1;
 mainShmemPtr->dropOffCommands[trno] = -1;

 if (pkgCount == 0) {
 int bst = -1;
 int bstSc = INT_MIN;

 for (int i = 0; i < totPckgs; i++) { // choosing next packaeg to pikc
 if (pckgs[i].pckUp == 0) {
 int dstc = abs(trx - pckgs[i].rqst.pickup_x) + abs(try - pckgs[i].rqst.pickup_y);
 int ttl = pckgs[i].rqst.expiry_turn - resForTurn.turnNumber;
 int sc = (ttl >= 0 ? ttl : 0) - 500 * dstc;

 if (sc > bstSc) {
 bstSc = sc;
 bst = i;
 }
 }
 }

 if (bst == -1) {
 strcpy(mainShmemPtr->authStrings[trno], "");
 continue;
 }

 PackageRequest *pr = &pckgs[bst].rqst; 
 pckgs[bst].trAsgnd = trno;

 if (trx == pr->pickup_x && try == pr->pickup_y) {
 mainShmemPtr->pickUpCommands[trno] = pr->packageId;
 pckgs[bst].pckUp = 1;
 mainShmemPtr->truckMovementInstructions[trno] = 's';
 } else {
 if (trx < pr->pickup_x)
 mainShmemPtr->truckMovementInstructions[trno] = 'r';
 else if (trx > pr->pickup_x)
 mainShmemPtr->truckMovementInstructions[trno] = 'l';
 else if (try < pr->pickup_y)
 mainShmemPtr->truckMovementInstructions[trno] = 'd';
 else if (try > pr->pickup_y)
 mainShmemPtr->truckMovementInstructions[trno] = 'u';
 else
 mainShmemPtr->truckMovementInstructions[trno] = 's';
 }

 strcpy(mainShmemPtr->authStrings[trno], "");
 } else {
 int fnlIdx = -1;
 for (int i = 0; i < totPckgs; i++) {
 if (pckgs[i].trAsgnd == trno && pckgs[i].pckUp == 1) {
 fnlIdx = i;
 break;
 }
 }

 if (fnlIdx == -1) {
 strcpy(mainShmemPtr->authStrings[trno], "");
 continue;
 }

 PackageRequest *pr = &pckgs[fnlIdx].rqst;

 if (trx == pr->dropoff_x && try == pr->dropoff_y) {
 mainShmemPtr->dropOffCommands[trno] = pr->packageId;
 pckgs[fnlIdx].pckUp = 2;
 mainShmemPtr->truckMovementInstructions[trno] = 's';
 } else {
 if (trx < pr->dropoff_x)
 mainShmemPtr->truckMovementInstructions[trno] = 'r';
 else if (trx > pr->dropoff_x)
 mainShmemPtr->truckMovementInstructions[trno] = 'l';
 else if (try < pr->dropoff_y)
 mainShmemPtr->truckMovementInstructions[trno] = 'd';
 else if (try > pr->dropoff_y)
 mainShmemPtr->truckMovementInstructions[trno] = 'u';
 else
 mainShmemPtr->truckMovementInstructions[trno] = 's';
 }

 authStrFrSolver(trno, pkgCount, mainShmemPtr->authStrings[trno]);
 }
 }

 TurnReadyRequest rdyMes = {0};
 rdyMes.mtype = 1;
 if (msgsnd(mainMesQid, &rdyMes, sizeof(rdyMes) - sizeof(long), 0) == -1){
 perror("Error in sending message");
 return 1;
 }


 }

 if (shmdt(mainShmemPtr) == -1) {
 perror("Error in detaching from shared memory");
 return 1;
 }

 return 0;
}
