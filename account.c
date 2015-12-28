#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "account.h"
#define DEBUG /*printf("%s %d\n", __FILE__, __LINE__)*/

//This is the actual memory structure of the bank
//It consists of accounts which hold the relevant data like name, balance, and the in-session flag.

//STRUCT CREATORS:
account* newAccount(char* name){

	account* newAccount = (account *)malloc(sizeof(account));
	newAccount->name = (char *)malloc(100);
	strcpy(newAccount->name, name);
	newAccount->balance = 0.0;
	newAccount->inSessionFlag = 0;
	return newAccount;	
}

void freeAccount(account* account){
	free(account->name);
	free(account);
}

account** newBank(){
	account** bank = (account **)malloc(20*sizeof(account *));
	account** temp = bank;
	int i;
	for(i=0; i<20; i++){
		*temp = NULL;
	}
	return bank;
}

/*
void freeBank(){
	account** temp = bank;
	int i;
	for(i=0; i<banksize; i++){
		free(*temp);
		temp++;
	}
}
*/
//PRINTS:
char* printAccount(account* account){
	if(account == NULL){
		return NULL;
	}
	char* ret = NULL;
	printf("%s - %.2f", account->name, account->balance);
	if(account->inSessionFlag == 1){
		printf(" - IN SERVICE\n");
	} else { 
		printf("\n");
	}
	return ret;	
}

char* printBank(account* bank[]){
	//DEBUG;
	account** temp = bank;
	char* ret = NULL;
	int i;
	printf("---------------\n");
	//DEBUG;
	for(i=0; i<20; i++){
		//DEBUG;
		printAccount(bank[i]);
		temp++;
	}
	//DEBUG;
	printf("---------------\n");
	return ret;
}

//HELPERS:
//This adds an account to the bank and will also error if the bank is full
account* addAccount(account* bank[], int banksize, char* name){
	DEBUG;
	if(banksize == 20){
		return NULL;
	}
	DEBUG;
	account* acc = newAccount(name);
	DEBUG;
	account** temp = bank;
	int i;
	for(i=0; i<20; i++){
		DEBUG;
		if(bank[i]==NULL){
			DEBUG;
			bank[i] = acc;
			banksize++;
			return acc;
		}
		temp++;
	}
	DEBUG;
	return NULL;

}
//This gets what index in the bank a certain account name is in
//It will also error if the account is not in the bank
int getIndex(account* bank[], int banksize, char* accountname){
	DEBUG;
	int index;
	for(index = 0; index < 20; index++){
		DEBUG;
		if(bank[index] != NULL && strcmp(bank[index]->name, accountname) == 0){
			DEBUG;
			return index;
		}
	}
	return -1;
	
}


//INTERACTIONS:
//This will remove a certain amount of money from the balance of a designated account
//It will error on invalid amounts.
int debit(account* bank[], int banksize, float amount, char* accountname){
	if(!accountname || amount <= 0){
		return -1;
	}
	
	int index = getIndex(bank, banksize, accountname);

	if(bank[index]->balance < amount){
		return -1;
	}
	
	bank[index]->balance -= amount;	
	return 1;

}

//This will give a certain amount of money to the balance of a designated account
//It will error on invalid amounts
int credit(account* bank[], int banksize, float amount, char* accountname){
	DEBUG;
	//printf("banksize: %d, amount: %f, accountname: %s\n", banksize, amount, accountname);
	if(accountname == NULL || amount <= 0){
		DEBUG;
		return -1;
	}	
	DEBUG;
	int index = getIndex(bank, banksize, accountname);
	//printf("%d %s\n", index, accountname);
	bank[index]->balance = bank[index]->balance + amount;
	return 1;
}

//This gets the balance of a designated account
float balance(account* bank[], int banksize, char* accountname){
	if(accountname == NULL){
		return -1;
	}
	int index = getIndex(bank, banksize, accountname);
	
	return bank[index]->balance;	

}



