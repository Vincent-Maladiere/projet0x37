#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include "midi.h"
#include <math.h>

extern int16_t divnoire; // division de la noire 240 >> 1 seconde

void affiche(liste L){ // pour tester dans le programme
	liste p;
	for(p=L;p;p=p->suiv){
		printf("%d %f\n",p->note,p->duree);
	}
}

/*On effectue des fusions de plusieurs fichiers,
un fichier pour l'entéte principale (MTHD)
un fichier pour l'entéte d'un track (MtrK)
un fichier pour les données du track (data)

la fusion des 3 donnant le fichier midi final

car on doit indiquer les nombres d'octets contenus dans les données du track, il suffira d'ajouter la taille du fichier data a la fin du fichier Mtrk pour compléter l'entéte du track et de fusionner tout les fichiers pour completer le fichier midi .

On aurait pu utiliser les pointeurs associés aux fichiers , les déplacer et écrire les octets length au bon endroit ( c'est a dire a 18 octets par rapport au debut du fichier ) a l'aide de la fonction fseek et fwrite.

Notes:
	l'entéte du mtrk est de la forme
		MTrk(4 octets) + Length(4 octets) + data( Length octets)

	l'entéte du MthD est de la forme
		MthD(4 octets) + length(4 octets) + format(2 octets) +tracks(2 oct) + division(2 oct)



Important: rajouter des vérifications pour les appels des fonctions fseek, calloc , fputc , fgetc , fwrite , fopen , remove etc ...

*/


FILE * midihead( char * nompartition ){		//OK
	char t[12]={0x4d,0x54,0x68,0x64,0x00,0x00,0x00,0x06,0x00,0x01,0x00,0x01}; // ici on a mis le nombre de division à 0xF0= 240 , peut être provisoire
	FILE * midibin;
	int16_t swappeddivnoire;
	midibin = fopen(nompartition,"w+b");
	fwrite(t,sizeof(char),sizeof(t),midibin);
	swappeddivnoire= ((divnoire<<8)&0xff00) | ((divnoire>>8)&0x00ff) ;
	fwrite(&swappeddivnoire,sizeof(swappeddivnoire),1,midibin);
	return midibin;
}

FILE * miditrackhead( char * trackhead ){	//OK	
	char t[4] = {0x4d,0x54,0x72,0x6b}; // "Mtrk" est écrit , la longueur sera écrite plus tard
	FILE* miditrackhead;
	miditrackhead = fopen(trackhead,"w+b");
	fwrite(t,sizeof(char),4,miditrackhead);
	return miditrackhead;
}




/*l'algorithme derriére cette fonction est le suivant:
	Au temps ti
	on fait appel a noteon avec la liste Li du temps ti
	on fait une concatenation entre Si-1 et Li , la liste obtenue est appelé Li
	on effectue un tri au sein de cette liste
	on sépare la liste en fonction des durées, ce qui donne Si
	on fait appel au note off sur les notes restantes dans Li
		si on est pas a la dernière étape:
			on fait appel a la fonction delay pour completer le temps entre un noteoff et un nouvel note on
			on passe Si au prochain cycle de boucle ti+1
		sinon on cloture le fichier
	*/

FILE * miditrackdata( Tnote tab , int tailletab , char * trackdata ){
	FILE * miditrackdata=fopen(trackdata,"w+b");
	int i;
	liste Li=NULL;
	liste Si=NULL;
	liste p;
	double tau;
	double taudelay;
	char t[1] = {0x00};
	fwrite(t,sizeof(char),1,miditrackdata);
	for(i=0;i<tailletab;i++){
		if(i == tailletab-1 && tab[i].temps>0){
			Li=tabtoliste(tab[i].tabchord);
			noteon(Li,miditrackdata);
			Li=concat(Li,Si);
			Li=tri(&Li);
			//freeliste(&Si); bug a corriger
			noteoff(Li,Si,miditrackdata);
			freeliste(&Li);
		}
		else{
			if( (tab[i].temps > 0 && i!=0) || i==0){
				Li=tabtoliste( tab[i].tabchord );
				noteon(Li,miditrackdata);
				Li=concat(Li,Si);
				Li=tri(&Li);
				
				if(tab[i+1].temps  <= 0 ){
					tau=0.99;
				}
				else tau = tab[i+1].temps-tab[i].temps;
				
				Si = split(&Li,tau);
				if(Li){ // si Li est non NULL il y a des notes à éteindre
					for( p=Li ; p->suiv ; p=p->suiv );
					taudelay = tau - p->duree ; // a faire avant le noteoff ET le split
					noteoff( Li , Si , miditrackdata ); // l'ajustement temporel des listes s'effectue dans note off et delay
					delay( Si , taudelay , miditrackdata) ;
					freeliste(&Li);
				}
				else{ // si Li est NULL il y a aucune note à éteindre , on comble le délai a l'aide de delay.
					taudelay = tau;
					delay(Si,taudelay,miditrackdata);
				}
			}
		}
	}
	return miditrackdata;
}

FILE * endmiditrack( FILE * miditrackhead , FILE * miditrackdata , char * track ){
	FILE * miditrack;
	int size;
	int swappedsize;
	char end[3] = {0xff,0x2f,0x00};
	fseek(miditrackhead,0,SEEK_END);
	size=(int)tailletrack(miditrackdata);
	size+=3; // on rajoute 3 octets pour la cloture du fichier par ff 2f 00
	swappedsize= ((size>>24)&0xff) | ((size<<8)&0xff0000) | ((size>>8)&0xff00) | ((size<<24)&0xff000000); // conversion big endian / little endian
	fwrite(&swappedsize,sizeof(swappedsize),1,miditrackhead);
	miditrack=mergeandclose(miditrackhead,miditrackdata,track);
	fwrite(end,sizeof(char),sizeof(end),miditrack); // on écrit ff 2f 00 pour la fin du track
	return miditrack;
}


FILE * endmidi( FILE * midihead , FILE * miditrack , char * midiname ){
	return mergeandclose( midihead , miditrack , midiname);
}


void noteon( liste Li , FILE * midibin ){
	liste p=Li;
	char t[4] = {0x90,Li->note,0x64,0x00};
	char c[3] = {0x90,00,0x64};
	// on fixe la vélocité a 64 , c'est pas trés important , 0x90 signifie noteon , les appels des notes sont simultanés d'où le délai fixé a 0x00
	if(!p->suiv){
		c[1] = p->note; // on est au dernier élément , on ne met pas de délai
		fwrite(c,sizeof(char),sizeof(c),midibin);
	}
	else {
		for( p=Li ; p->suiv ; p=p->suiv ){
				t[1] = p->note; 
				fwrite(t,sizeof(char),sizeof(t),midibin);
		}
		c[1] = p->note; // on est au dernier élément , on ne met pas de délai
		fwrite(c,sizeof(char),sizeof(c),midibin);
	}
}

void noteoff( liste Li , liste Si , FILE * miditrackdata ){ // toujours de la forme {délai de la note i} + {noteoff note i}	
	char* t;
	char* c;
	liste p;
	double tau;
	bufferc buff=NULL;
	t=calloc(1,sizeof(*t));
	c=calloc(3,sizeof(*c));
	t[0] = 0; // on initialise t et c , t pour le mode délai et c pour le mode action (note off)
	if(Li){ // si jamais Li est NULL , probablement impossible
		c[0]=0x80;
		c[1]=Li->note;
		c[2]=0x00;
		for(p=Li;p;p=p->suiv){
			//le premier octet écrit correspond au délai de la 1 ére note
			tau=p->duree;
			buff=convert_128(tau);// on met dans un buffer les octets correspondant au mode delai
			ajustement(Li,tau); // on oublie pas d'ajuster les durées des autres notes qui doivent étre éteintes plus tard
			ajustement(Si,tau);
			do{ // on rentre dans une boucle si jamais le nb d'octets a écrire  pour le délai est plus grand que 1 
				t[0]=(char)buff->r;
				fwrite(t,sizeof(char),1,miditrackdata);
				buff=supprimer_tetebuff(buff); // une fois la donnée écrite on la supprime , elle n'est pas utile pour la suite
			}
			while(buff);
			// aprés le délai de la note , on rentre dans le mode action et on l'éteint
			c[1]=p->note;
			fwrite(c,sizeof(char),3,miditrackdata);
		}
	}
}

long int tailletrack(FILE * miditrackdata){
	long int size;
	fseek(miditrackdata,0,SEEK_END); // on se positionne a la fin du fichier a l'aide de la fonction fseek
	size=ftell(miditrackdata); // ftell retourne le nombre d'octets depuis le debut du fichier 
	return size;
}

	

liste split( liste *Li , double taumax ){ // retourne NULL si il n' y a pas de chevauchement temporel de notes
	liste p=*Li;
	liste a;
	if(!p) return NULL;
	if(taumax<0){
		printf("problème appel fonction split\n");
		return NULL;
	}
	if(p==*Li && p->duree >taumax){
		*Li=NULL;
		return p;
	}
	for(p=*Li; p->suiv && p->suiv->duree <= taumax;p=p->suiv);
	a=p->suiv;
	p->suiv=NULL;
	return a;	
}

void ajustement( liste Li , double tau ){
	liste pl;
	if(Li){	// n'ajuste rien si Li=NULL
		for( pl=Li; pl; pl=pl->suiv){
			if( pl->duree - tau >0 )pl->duree-=tau;
			else pl->duree=0;
		}
	}
}

bufferc convert_128( double tau ){ // a revoir on doit ajouter 128 a tout les octets sauf le dernier
	bufferc buf=NULL;
	int R;
	int Q = 1;
	double N = floor(tau*divnoire);
	int p = 1;
	while(Q != 0){
		R = (int)N%128;
		Q = (int)N/128;
		if(p == 1){ // pour avoir le premier octet correspondant a 128**0*R
			buf = ajout_tetebuff((char)(R),buf);
			p = 0;
		}
		else {
		  buf = ajout_tetebuff((char)(R+128),buf); // pour avoir les autres octets correspondants a 128**N*(R-128) ( c'est bien un + dans le else) voir le mode délais d'un MIDI
		}
		N = Q;
	}
	return buf;
}

void delay( liste Si , double taumax , FILE * miditrackdata ){
	char * c;
	bufferc buff;
	if(taumax>=0){
		buff=convert_128(taumax);
		c=calloc(1,sizeof(*c));
		do{
			c[0]=buff->r;
			fwrite(c,sizeof(char),1,miditrackdata);
			buff=supprimer_tetebuff(buff);
		}
		while(buff);
		ajustement(Si,taumax); // on ajuste bien les durées des notes qui doivent étre eteintes au prochain noteoff
	}
}


FILE * mergeandclose(FILE * f1, FILE * f2, char * filename){ // on met f2 aprés f1 , le fichier obtenu est f3 , on renvoit le pointeur
	FILE * f3;
	int c;
	fseek(f1,0,SEEK_SET);
	fseek(f2,0,SEEK_SET);
	f3=fopen(filename,"w+b");
	for(c=fgetc(f1);c!=EOF;c=fgetc(f1)){
		fputc(c,f3);
	}
	for(c=fgetc(f2);c!=EOF;c=fgetc(f2)){
		fputc(c,f3);
	}
	fclose(f1);
	fclose(f2);
	return f3;
}


void mainmidi(char * midiname,Tnote tab,int tailletab){
	FILE * midiheadf=midihead("midihead");
	FILE * miditrackheadf=miditrackhead("trackhead");
	FILE * miditrackdataf=miditrackdata(tab,tailletab,"trackdata");
	FILE * midi=endmiditrack(miditrackheadf,miditrackdataf,"miditrack");
	midi=endmidi(midiheadf,midi,midiname);
	remove("midihead"); // la suppression des fichiers sera enlevé lors des tests , on peut éventuellement mettre cette option en paramètre
	remove("trackhead");
	remove("trackdata");
	remove("miditrack");
	fclose(midi);
}

	
