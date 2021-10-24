#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Basic.h"
#include "cunit_interface.h"  

static CU_pSuite pSuite = NULL;;

// �����ʱ
void Delay(__IO uint32_t nCount)
{
  for(; nCount != 0; nCount--);
} 

#if 0
static void test_case1(void) 
{ 
	CU_ASSERT(1); 
}
static void test_case2(void) 
{ 
	CU_ASSERT_EQUAL(1, 2); 
}
static void test_case3(void) 
{ 
	CU_ASSERT_STRING_EQUAL("abc", "edf"); 
}
#endif

/**
  * @brief  Main program.
  * @param  None
  * @retval : None
  */

cunit_status_t cunit_init(void)
{
	CU_ErrorAction error_action = CUEA_IGNORE;
	/*.��ʼ��ע���*/
	if (CU_initialize_registry()) {
		printf("\nInitialization of Test Registry failed.\r\n");
		return CUNIT_FAIL;
	}
	/*.��Ӳ��Լ�*/
	if (pSuite == NULL) {
		pSuite = CU_add_suite("suite_for_test", NULL, NULL);
		if (NULL == pSuite) {
			return CUNIT_FAIL;
		}
	}
	/*.���ò���ģʽ������ѡ������ģʽ�������̶ȵ��������ϸ��*/
	CU_basic_set_mode(CU_BRM_VERBOSE);

	/*.���÷���������action������ѡ������ܲ���**/
	CU_set_error_action(error_action);

	return CUNIT_SUCCESS;
}

void cunit_deinit()
{
	/*����ע��㣬�ͷ��ڴ�*/
	CU_cleanup_registry();
	pSuite = NULL;
}

cunit_status_t cunit_add_test_case(const char *case_name, cunit_test_case_func_t test_case)
{
	CU_pTest result = NULL;
	if (pSuite == NULL) {
		return CUNIT_FAIL;
	}
	result = CU_add_test(pSuite, case_name, test_case);
	if(result == NULL) {
		return CUNIT_FAIL;
	}
	return CUNIT_SUCCESS;
}

cunit_status_t cunit_test_start()
{
	CU_ErrorCode result = CUE_SUCCESS;
	/*.��ʼ����*/
	result = CU_basic_run_tests();
	printf("Tests completed with return value %d.\r\n", result);
	if (result != CUE_SUCCESS) {
		return CUNIT_FAIL;
	}
	return CUNIT_SUCCESS;
}

#if 0
int cunit_init(void)
{
	CU_ErrorAction error_action = CUEA_IGNORE;
	int test_return = 0;
	/*1.��ʼ��ע���*/
	if (CU_initialize_registry()) {
		printf("\nInitialization of Test Registry failed.\r\n");
		return -1;
	}
	/*2.��Ӳ��Լ�*/
	if (pSuite == NULL) {
		pSuite = CU_add_suite("suite_for_test", NULL, NULL);
		if (NULL == pSuite) {
			return -1;
		}
	}
	/*3.��Ӳ�������*/
	CU_add_test(pSuite, "test_case1", test_case1);
	CU_add_test(pSuite, "test_case2", test_case2);
	CU_add_test(pSuite, "test_case3", test_case3);
	/*4.���ò���ģʽ������ѡ������ģʽ�������̶ȵ��������ϸ��*/
	CU_basic_set_mode(CU_BRM_VERBOSE);
	/*5.���÷���������action������ѡ������ܲ���**/
	CU_set_error_action(error_action);
	/*6.��ʼ����*/
	test_return = CU_basic_run_tests();
	printf("\nTests completed with return value %d.\n", test_return);
	/*7.����ע��㣬�ͷ��ڴ�*/
	CU_cleanup_registry();

	return 0;
}

#endif

/******************* (C) COPYRIGHT 2012 WildFire Team *****END OF FILE************/
