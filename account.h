#include <stdio.h>

typedef struct account{

	char* name;
	float balance;
	int inSessionFlag;

} account;

account* newAccount(char* name);
account* addAccount(account* bank[], int numAccounts, char* name);
void freeAccount(account* account);
void freeBank();

int debit(account* bank[], int banksize, float amount, char* accountname);
int credit(account* bank[], int banksize, float amount, char* accountname);
float balance(account* bank[], int banksize, char* accountname);
int getIndex(account* bank[], int banksize, char* accountname);
char* printBank(account* bank[]);
account** newBank();
