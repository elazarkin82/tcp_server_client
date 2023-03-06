/*
 * testcase_interface.h
 *
 *  Created on: 3 Mar 2023
 *      Author: elazarkin
 */

#ifndef TESTS_TEST_CASES_TESTCASE_INTERFACE_HPP_
#define TESTS_TEST_CASES_TESTCASE_INTERFACE_HPP_

class BaseTestInterface
{
public:
	virtual ~BaseTestInterface() {};
	virtual bool test() = 0;
};



#endif /* TESTS_TEST_CASES_TESTCASE_INTERFACE_HPP_ */
