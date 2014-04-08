/* examples/C/ssids.c - simple code to illustrate use of SSIDS */

#include "spral.h"

void main(void) {
   /* Derived types */
   void *akeep, *fkeep;
   struct spral_ssids_options options;
   struct spral_ssids_inform inform;

   /* Initialize derived types */
   akeep = NULL; fkeep = NULL; /* Important that these are NULL to start with */
   spral_ssids_default_options(&options);

   /* Data for matrix:
    * ( 2  1         )
    * ( 1  4  1    1 )
    * (    1  3  2   )
    * (       2      )
    * (    1       2 ) */
   bool posdef = false;
   int n = 5;
   int ptr[]    = { 1,        3,             6,      8,8,   9 };
   int row[]    = { 1,   2,   2,   3,   5,   3,   4,   5   };
   double val[] = { 2.0, 1.0, 4.0, 1.0, 1.0, 3.0, 2.0, 2.0 };

   /* The right-hand side with solution (1.0, 2.0, 3.0, 4.0, 5.0) */
   double x[] = { 4.0, 17.0, 19.0, 6.0, 12.0 };

   /* Perform analyse and factorise with data checking */
   bool check = true;
   ssids_analyse(check, n, NULL, ptr, row, NULL, &akeep, &options, &inform);
   if(inform.flag<0) {
      ssids_free(&akeep, &fkeep);
      exit(1);
   }
   ssids_factor(posdef, NULL, NULL, val, NULL, akeep, &fkeep, &options,
         &inform);
   if(inform.flag<0) {
      ssids_free(&akeep, &fkeep);
      exit(1);
   }

   /* Solve */
   call ssids_solve(x,akeep,fkeep,options,inform)
   if(inform.flag<0) {
      ssids_free(&akeep, &fkeep);
      exit(1);
   }
   printf("The computed solution is:\n");
   for(int i=0; i<n; i++) printf(" %18.10e") x[i];
   printf("\n");

   /* Determine and print the pivot order */
   /*int piv_order[5];
   call ssids_enquire_indef(akeep, fkeep, options, inform, piv_order=piv_order)
   write(*,"(a,10i5)") ' Pivot order:', piv_order(1:n)*/

   int cuda_error = ssids_free(akeep, fkeep);
   if(cuda_error!=0) exit(1);
}
