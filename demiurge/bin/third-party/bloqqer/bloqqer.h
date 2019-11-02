#ifndef BLOQQER_H_INCLUDED
#define BLOQQGER_H_INCLUDED



typedef enum VarValue {
  UNKN = 0,
  POS = 1,
  NEG = 2,
  DONTCARE = 3,
} VarValue;



/** add a new variable var to prefix of QBF 
  * if var > 0 ... existentially quantified
  * if var < 0 ... universally quantified 
  */
void bloqqer_decl_var ( int );


/** add another literal to current clause
  * 0 indicates end of clause 
  */
void bloqqer_add ( int );


/** do preprocessing 
  */
int bloqqer_preprocess ();


/** set bloqqer to initial state 
  */
void bloqqer_reset ();


/** call depqbf to solve the formula 
  */
int bloqqer_solve ();


/** print the formula 
 */
void bloqqer_print ();


/** reset the iterator to get all literals of a formula 
  */
void bloqqer_reset_lit_iterator ();


/** get next literal 
  */
int bloqqer_lit_iterator_next ();


/** set bloqqer's options 
 */
void bloqqer_set_option(char * arg);


/** init bloqqer
  * m ... number of vars
  * n ... number of clauses 
  */
void bloqqer_init (int m, int n);

/** parse input form file iname 
  */
void bloqqer_parse (char * iname);

/** print statistics
  */
void bloqqer_stats ();

/** free memory */
void bloqqer_release ();

/** get value of a variable */
VarValue bloqqer_getvalue (int v);

#endif


