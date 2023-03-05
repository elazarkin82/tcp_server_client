/*
 * testcase_interface.h
 *
 *  Created on: 3 Mar 2023
 *      Author: elazarkin
 */

#ifndef TESTS_TEST_CASES_TESTCASE_INTERFACE_H_
#define TESTS_TEST_CASES_TESTCASE_INTERFACE_H_

class BaseTestInterface
{
public:
	BaseTestInterface(){}
	virtual ~BaseTestInterface(){};
	virtual bool test();
};



#endif /* TESTS_TEST_CASES_TESTCASE_INTERFACE_H_ */
