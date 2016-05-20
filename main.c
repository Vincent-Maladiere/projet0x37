#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "audio/audioprocess.h"
#include "traitement/fonctions.h"
#include "midi/midi.h"
#include "textexport/textexport.h"
#include "traitement/fftw3.h"


double facteurmoyenne = 1;
double thresv0 = THRESV0;
double thresvi = THRESVI;
FILE * logfile;
int blog = 0;

int main(int argc, char** argv){
	double * datain;
	int  size;
	double duration;
	double samplerate;
	int sizeframe = 8192 ;
	int sizeTmax;
	int b;
	int c;
	Tnote T;	
	//char s[255];
	//int index;
	
	opterr = 0;

	if(argc == 0){
		fprintf(stderr,"Le programme nécessite un nom de fichier audio appelé avec -f");
	}
	
	while((c=getopt(argc,argv,"v:w:m:f:lh")) != -1){
		switch (c)
		{
			case 'v' :
				thresv0 = (double)atof(optarg);
				break;
			case 'w' :
				thresvi = (double)atof(optarg);
				break;
			case 'm' : 
				facteurmoyenne = (double)atof(optarg);
				break;
			case 'l' :
				blog = 1;
				break;
			case 't' :
				b = (int)atof(optarg);
			case 'h' :
				break;
			case 'f' :
				datain=mainaudio(optarg,&size,&samplerate);
				break;
			case '?' :
				if(optopt == 'v' || optopt == 'w' || optopt == 'm') fprintf (stderr, "Option -%c nécessite un argument.\n", optopt);
				else if ( isprint(optopt) ) fprintf (stderr, "Option inconnue `-%c.\n", optopt);
				else fprintf(stderr,"Caractère d'option inconnue`\\x%x'.\n",optopt);
				return 1;
			default:
				abort();
		}
	}
	/*	
	printf("Entrer le nom du fichier audio\n");
	fgets(s,255,stdin);
	s[strcspn(s, "\r\n")] = 0;	
	printf("Importation du fichier audio ...\n");
	*/
	
	//datain=mainaudio(s,&size,&samplerate);
	if(!datain) return 0;
	duration=(double)size/samplerate;
	/*
	printf("Le fichier audio dure %lf s , il comporte %d éléments, la fréquence d'échantillonage est de %lf\n",duration,size,samplerate);
	puts("Traitement du fichier :");
	printf("Quelle taille de porte voulez vous choisir ?\n\n");
	printf(" 1 : 2**11 soit %lf s\n",(double)2048/samplerate);
	printf(" 2 : 2**12 soit %lf s\n",(double)4096/samplerate);
	printf(" 3 : 2**13 soit %lf s\n",(double)8192/samplerate);
	printf(" 4 : 2**14 soit %lf s\n",(double)16384/samplerate);
	scanf("%d",&b);
	*/
	switch (b)
	{
	case 1 : 
		sizeframe = 2048;
		break;
	case 2 : 
		sizeframe = 4096;
		break;
	case 3 : 
		sizeframe = 8192;
		break;
	case 4 : 
		sizeframe = 16384;
		break;
	default :
		sizeframe = 4096;
		break;
	}
	/*

	printf("Vous avez choisi une largeur de %lf s .\n", (double)sizeframe/samplerate);
	printf("La résolution temporelle de la partition est de %lf s\n",(double)sizeframe/(2*samplerate));

	printf("Lors du traitement une moyenne courante est calculée sur le spectre de Fourier\n");
	printf("On peut être amené à ajuster légéremment cette moyenne afin d'obtenir une meilleur partition\n");
	printf("Facteur d'ajustement ?\n( valeure positive, 1 par défaut )\n");
	scanf("%lf",&facteurmoyenne);
	printf("Facteur d'ajustement : %lf\n\n\n",facteurmoyenne);

	
	puts("Maintenant, il est nécessaire de choisir deux valeurs, Thresv0 et Thresvi, elles permettent de distinguer les notes 'fantômes' des notes réelles\n");
	puts("Les notes sont d'autant plus filtrées que ces valeurs sont importantes, la condition de détection sur Thresv0 est prioritaire à celle sur Thresvi\n");
	puts("On peut tout d'abord commencer par des valeurs importantes pour les deux, puis les diminuer jusqu'à obtenir une bonne partition");
	puts("Il peut être interessant de regarder le fichier log, afin d'avoir une idée des valeurs de thresv0 et thresvi");
	puts("Mais en général thresv0 et thresvi sont respectivement aux alentours de 5 et 10");
	puts("( Ces valeurs dépendent du facteur d'ajustement précédent)\n");
	puts("Entrer la valeur de Thresv0");
	scanf("%lf",&thresv0);
	puts("Entrer la valeur de Thresvi");
	scanf("%lf",&thresvi);
	printf("Thresv0 : %lf\nThresvi : %lf\n\n",thresv0,thresvi);
	
	
	puts("Voulez vous récupérer un fichier log récapitulant le traitement du fichier ?");
	puts(" 1 : OUI    0 : NON");
	scanf("%d",&blog);
	*/
	
	if(blog){
		logfile=fopen("log","w");
		fprintf(logfile,"Log projet0x37.exe\nFréquence d'échantillonnage : %lf  Largeur : %lf\n DuréeThresv0 : %lf\nThresvi : %lf\nVaration minimale de vi : %lf\nNombre d'itération maximale : %d\n",samplerate,duration,thresv0,thresvi,DELTAMIN,IMAX);
	}

	if( facteurmoyenne < 0 ) facteurmoyenne =1;
	sizeTmax = 2*size/sizeframe + 2;

	T=calloc(sizeTmax,sizeof(*T));

	mainprocessing( T ,  sizeTmax , datain , size  , samplerate , sizeframe);
	if(blog)fclose(logfile);
	//for(i=0;i<sizeTmax;i++) printf("temps : %lf\n",T[i].temps);
	mainmidi("outputmidi",T,sizeTmax);
	T=simplifT(T,sizeTmax);
	//T=condT(T,sizeTmax,0.101); // proto
	mainmidi("outputmidisimplifie",T,sizeTmax);
	return 0;
}

