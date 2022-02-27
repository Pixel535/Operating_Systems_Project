#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#define N 100

int syg1;
int syg2;
int syg3;

//zdefiniowanie desryptorow ktore pozwalaja na uzycie pipe
int deskryptor_1[2];
int deskryptor_2[2];
int deskryptor_3[2];

//zdefiniowanie semaforow
sem_t *p1;
sem_t *p2;
sem_t *p3;
sem_t *menu;

//zdefiniowanie zmiennych przechowujacych pid-y proceosow
int pidp3 [1];
int pidp2 [1];
int pidp1 [1];

int* PidP3;
int* PidP2;
int* PidP1;

//zdeinowanie struktur funcji sigaction (do obslugi sygnalow)
struct sigaction PM;
struct sigaction P1;
struct sigaction P2;
struct sigaction P3;
struct sigaction Pd;

//zdefiniowanie maski dla kazdego procesu
sigset_t mask1;
sigset_t mask2;
sigset_t mask3;

//hendler oblsugujacy sygnaly dla PM
void handlerPM(int sig, siginfo_t *info, void *uncontext){
	//oczewianie na dostaie odpoweidego sygnalu od procesu 3
	if(*PidP3 == info -> si_pid && sig == SIGTSTP){
		//wpisanie wartosci sygnalu do pipe
		write(deskryptor_1[1],&sig,1);
		write(deskryptor_2[1],&sig,1);
		write(deskryptor_3[1],&sig,1);
		//powiadomienie procesu 1 o odczytaniu piepe (SIGUSR1)
		kill(*PidP1,SIGUSR1);

	}

	else if((sig == SIGCONT || sig == SIGTERM) && *PidP3 == info -> si_pid){
		//wpisanie wartosci sygnalu do pipe
		write(deskryptor_1[1],&sig,1);
		write(deskryptor_2[1],&sig,1);
		write(deskryptor_3[1],&sig,1);
		//powiadomienie procesu 1 o odczytaniu piepe (SIGUSR2)
		kill(*PidP1,SIGUSR2);
	}


	/*
	else if(sig == SIGTERM && *PidP3 != info -> si_pid){
		printf("\n");
		sem_close(p1);
		sem_close(p2);
		sem_close(p3);
		sem_unlink("Nazwa2");
		sem_unlink("Nazwa");
		sem_unlink("Nazwa3");
		kill(0,SIGKILL);
		exit(0);
	}
	*/

}


//hendler oblsugujacy sygnaly dla P1
void handlerP1(int sig, siginfo_t *info, void *uncontext){
	//reakcja na dostanie sygnalu SIGUSR1 od PM
	if(sig == SIGUSR1 && info -> si_pid == getppid()){
		//odczytanie z pipe wartosci sygnalu
		read(deskryptor_1[0],&sig,1);
		//powiadomienie procesu 2 o odczytaniu piepe (SIGUSR1)
		kill(*PidP2, SIGUSR1);
		//wywoalanie na sobie odebranego sygnalu
		kill(getpid(),sig);
	}
	else if(sig == SIGUSR2 && info -> si_pid == getppid()){
		//odczytanie z pipe wartosci sygnalu
		read(deskryptor_1[0],&sig,1);
		//powiadomienie procesu 2 o odczytaniu piepe (SIGUSR2)
		kill(*PidP2, SIGUSR2);
		//zwolnienie maski
		sigemptyset(&mask1);
		sigprocmask(SIG_BLOCK, &mask1, NULL);
		//ustawienie podstawowej oblsugi syganlu
		sigaction(sig, &Pd, NULL);
		//wywolanie na sobie danego syganlu
		kill(getpid(),sig);
		//ustawinie hendlera do obslugi sygnalu
		sigaction(sig, &P1, NULL);
		}
	else if(sig == SIGTSTP && info -> si_pid == getpid()){
		//ustawienie maski na reszte sygnalow (zablokowanie odbiernia sygnalu)
		sigemptyset(&mask1);
		sigaddset(&mask1, SIGCONT | SIGTERM);
		sigprocmask(SIG_BLOCK, &mask1, NULL);
		//zapauzowanie prcesu
		pause();
	}
}

void handlerP2(int sig, siginfo_t *info, void *uncontext){
	if(sig == SIGUSR1 && info -> si_pid == *PidP1){
		//odczytanie z pipe wartosci sygnalu
		read(deskryptor_2[0],&sig,1);
		//powiadomienie procesu 3 o odczytaniu piepe (SIGUSR1)
		kill(*PidP3, SIGUSR1);
		//wywolanie na sobie odebranego sygnalu
		kill(getpid(),sig);

	}
	else if(sig == SIGUSR2 && info -> si_pid == *PidP1){
		//odczytanie z pipe wartosci sygnalu
		read(deskryptor_2[0],&sig,1);
		//powiadomienie procesu 3 o odczytaniu piepe (SIGUSR1)
		kill(*PidP3, SIGUSR2);
		sigemptyset(&mask2);
		sigprocmask(SIG_BLOCK, &mask2, NULL);
		sigaction(sig, &Pd, NULL);
		kill(getpid(),sig);
		sigaction(sig, &P2, NULL);

	}
	else if(sig == SIGTSTP && info -> si_pid == getpid()){
		//ustawienie maski na reszte sygnalow (zablokowanie odbiernia sygnalu)
		sigemptyset(&mask2);
		sigaddset(&mask2, SIGCONT | SIGTERM );
		sigprocmask(SIG_BLOCK, &mask2, NULL);
		//zapauzowanie prcesu
		pause();
	}
}

void handlerP3(int sig, siginfo_t *info, void *uncontext){
	//jezei sygnal SIGTSTP zostal wyslany do samego siebie zapauzowanie procesu
	if(sig == SIGTSTP && info -> si_pid == getpid()){
		pause();
	}
	//wyslanie otrzymanego sygnalu SIGTSTP do PM
	else if(sig == SIGTSTP){
		kill(getppid(),sig);
	}
	//wyslanie otrzymanego sygnalu SIGCONT do PM
	else if(sig == SIGCONT){
		kill(getppid(),sig);
	}
	//wyslanie otrzymanego sygnalu SIGTERM do PM
	else if(sig == SIGTERM){
		kill(getppid(),sig);
	}

	else if(sig == SIGUSR1 && info -> si_pid == *PidP2){
		//odczytanie z pipe wartosci sygnalu
		read(deskryptor_3[0],&sig,1);
		//wyslanie otrzymanego sygnalu do sameg siebie
		kill(getpid(),sig);
	}
	else if(sig == SIGUSR2 && info -> si_pid == *PidP2) {
		//odczytanie z pipe wartosci sygnalu
		read(deskryptor_3[0],&sig,1);
		sigemptyset(&mask3);
		sigprocmask(SIG_BLOCK, &mask3, NULL);
		sigaction(sig, &Pd, NULL);
		kill(getpid(),sig);
		sigaction(sig, &P3, NULL);
	}
}


int main(int argc, char *argv[])
{
	//bufor na wartosci przekazywane medzy procesami
	char buf_p1_p2[N];
	int buf_p2_p3[1];
	int l_z = 0; // liczba znakow

	int wybor;

	char j[1];
	int zgodnosc = 1;
	FILE *plik = NULL;

	//zdefiniowane pipe na przekazywanie sygnalow
	pipe(deskryptor_1);
	pipe(deskryptor_2);
	pipe(deskryptor_3);

	//zdefiniowanie pamieci wspoldzielonej na wartosci przekazywane medzy procesami
	char* shmemc = mmap(NULL,sizeof(buf_p1_p2),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int* shmemi = mmap(NULL,sizeof(buf_p2_p3),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	//zdefiniowanie pamieci wspoldzielonej do przechowywania pid-ow poszczegolnych procesow
	PidP3 = mmap(NULL,sizeof(pidp3),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	PidP2 = mmap(NULL,sizeof(pidp2),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	PidP1 = mmap(NULL,sizeof(pidp1),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	//zdefiniowanie semaforow do przekazywania danych miedzy procesami
	p1 = sem_open("Nazwa", O_CREAT, 0666, 1);
	p2 = sem_open("Nazwa2", O_CREAT, 0666, 0);
	p3 = sem_open("Nazwa3", O_CREAT, 0666, 0);
	menu = sem_open("Nazwa4", O_CREAT, 0666, 0);


	switch(fork())
	{
		case 0:

		//zamkniecie nie uzywanych pipe
		close(deskryptor_1[1]);
		close(deskryptor_2[0]);
		close(deskryptor_2[1]);
		close(deskryptor_3[0]);
		close(deskryptor_3[1]);
		//zdefiniowanie hendlera wykonujacego podstawowa funkcje signalu
		Pd;
		Pd.sa_handler = SIG_DFL;
		//zdefiniowanie hendlera wykonujacego nowa funkcje sygnalu
		P1;
		P1.sa_flags = SA_SIGINFO | SA_RESTART;
		//przypisanie funckji hendlera
		P1.sa_sigaction = &handlerP1;
		//przypisanie sygnalow do obslugi przez hendler
		sigaction(SIGINT, &P1, NULL);
		sigaction(SIGTSTP, &P1, NULL);
		sigaction(SIGCONT, &P1, NULL);
		sigaction(SIGTERM, &P1, NULL);
		sigaction(SIGUSR1, &P1, NULL);
		sigaction(SIGUSR2, &P1, NULL);
		//pobranie pidu procesu i wpisanie do pamieci wspoldzielonej
		pidp1[0] = getpid();
		memcpy(PidP1, pidp1, sizeof(pidp1));
		//oczekiwanie na powolanie wszytkich procesow
		sem_wait(menu);
		while(1){
			printf("PM: %d\n",getppid());
			printf("P1: %d\n",getpid());
			printf("P2: %d\n",*PidP2);
			printf("P3: %d\n",*PidP3);
			printf("Menu:\n");
			printf("1.Plik\n");
			printf("2.Stdin\n");
			printf("wybor: ");
			scanf("%d",&wybor);
			if(wybor == 1){
				//czytane zawartosci pliku txt
				plik = fopen("test.txt","r");
				while(fgets(buf_p1_p2,sizeof(buf_p1_p2),plik) != NULL){
					//synchronizacja z reszta procesow
					sem_wait(p1);
					memcpy(shmemc, buf_p1_p2, sizeof(buf_p1_p2));//zaladowanie pamieci wspoldzielonej
					sem_post(p2);
				}
			}
			//czyatnie wejscia stdin
			else if(wybor == 2){
				while(1)
				{
					if(zgodnosc == 1){
						gets(j);
					}
					zgodnosc = 0;
					//synchronizacja z reszta procesow
					sem_wait(p1);
					memset(buf_p1_p2,0,N);
					printf("pp1 (%d) - podaj tekst do wpisania: ",getpid());
					scanf("%[^\n]%*c",buf_p1_p2);

					if(buf_p1_p2[0] == '.'){
						//inforamcja od uzytkownika o checi wejscia do menu
						sem_post(p1);
						zgodnosc = 1;
						break;
					}
					//wpisanie danych do pamieci wspoldzielonej
					memcpy(shmemc, buf_p1_p2, sizeof(buf_p1_p2));
					sem_post(p2);
				}
			}

		}

		exit(0);
		case -1:
		printf("ERROR");
		exit(1);

		default:

		switch(fork())
		{
			case 0:
			//zamniecie nieuzywanych pipe
			close(deskryptor_2[1]);
			close(deskryptor_1[0]);
			close(deskryptor_1[1]);
			close(deskryptor_3[0]);
			close(deskryptor_3[1]);
			//zdefiniowanie hendlera wykonujacego podstawowa funkcje signalu
			Pd;
			Pd.sa_handler = SIG_DFL;
			P2;
			P2.sa_flags = SA_SIGINFO | SA_RESTART;
			P2.sa_sigaction = &handlerP2;
			sigaction(SIGINT, &P2, NULL);
			sigaction(SIGTSTP, &P2, NULL);
			sigaction(SIGCONT, &P2, NULL);
			sigaction(SIGTERM, &P2, NULL);
			sigaction(SIGUSR1, &P2, NULL);
			sigaction(SIGUSR2, &P2, NULL);
			pidp2[0] = getpid();
			memcpy(PidP2, pidp2, sizeof(pidp2));
			while(1)
			{
				//synchronizacja z reszta procesow
				sem_wait(p2);
				l_z = 0;
				//oblicznie dlugosci danego napisu (bez spacji)
				for(int i=0; i < strlen(shmemc); i++)
				{
					l_z ++;
					if(shmemc[i] == ' ')
						l_z --;
				}
				buf_p2_p3[0] = l_z;
				//wpisanie dlugosci do pamieci wspoldzielonej
				memcpy(shmemi, buf_p2_p3, sizeof(buf_p2_p3));
				sem_post(p3);
			}
			exit(0);

			case -1:
			printf("ERROR");
			exit(1);

			default:
			switch(fork())
			{
				case 0:
				close(deskryptor_3[1]);
				close(deskryptor_1[0]);
				close(deskryptor_1[1]);
				close(deskryptor_2[0]);
				close(deskryptor_2[1]);
				Pd;
				Pd.sa_handler = SIG_DFL;
				P3;
				P3.sa_flags = SA_SIGINFO | SA_RESTART;
				P3.sa_sigaction = &handlerP3;
				sigaction(SIGINT, &P3, NULL);
				sigaction(SIGTSTP, &P3, NULL);
				sigaction(SIGCONT, &P3, NULL);
				sigaction(SIGTERM, &P3, NULL);
				sigaction(SIGUSR1, &P3, NULL);
				sigaction(SIGUSR2, &P3, NULL);
				pidp3[0] = getpid();
				memcpy(PidP3, pidp3, sizeof(pidp3));
				sem_post(menu);
				while(1)
				{
					//synchronizacja oraz wypisanie dlugosci danego napisu
					sem_wait(p3);
					printf("pp3(%d) odczytealem %d\n",getpid(),*shmemi);
					sem_post(p1);
				}
				exit(0);

				case -1:
				printf("ERROR");
				exit(1);

				default:

				printf(" ");
			}
		}
	}
	close(deskryptor_1[0]);
	close(deskryptor_2[0]);
	close(deskryptor_3[0]);
	PM;
	PM.sa_flags = SA_SIGINFO | SA_RESTART;
	PM.sa_sigaction = &handlerPM;
	sigaction(SIGTSTP, &PM, NULL);
	sigaction(SIGCONT, &PM, NULL);
	sigaction(SIGTERM, &PM, NULL);
	//oczewkianie na zakonczenie dzialania procesu 3 (ostatniego)
	wait(*PidP3);
	//wylaczenie wszytkich semaforow oraz koniec programu
	printf("OFF\n");
	sem_close(p1);
	sem_close(p2);
	sem_close(p3);
	sem_unlink("Nazwa2");
	sem_unlink("Nazwa");
	sem_unlink("Nazwa3");
	sem_unlink("Nazwa4");
	return 0;
}
