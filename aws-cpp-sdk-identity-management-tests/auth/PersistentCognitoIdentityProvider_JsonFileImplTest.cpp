/*
  * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
  *
  * Licensed under the Apache License, Version 2.0 (the "License").
  * You may not use this file except in compliance with the License.
  * A copy of the License is located at
  *
  *  http://aws.amazon.com/apache2.0
  *
  * or in the "license" file accompanying this file. This file is distributed
  * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
  * express or implied. See the License for the specific language governing
  * permissions and limitations under the License.
  */


#include <aws/external/gtest.h>
#include <aws/testing/MemoryTesting.h>
#include <aws/identity-management/auth/PersistentCognitoIdentityProvider.h>
#include <aws/core/utils/FileSystemUtils.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <fstream>

using namespace Aws::Auth;
using namespace Aws::Utils;
using namespace Aws::Utils::Json;

static const char* FILENAME = ".identities";
static const char* DIR = ".aws";

Aws::String ComputeAwsDirPath()
{
    return FileSystemUtils::GetHomeDirectory() + DIR;
}

Aws::String ComputeIdentityFilePath()
{
    return ComputeAwsDirPath() + PATH_DELIM + FILENAME;
}

TEST(PersistentCognitoIdentityProvider_JsonImpl_Test, TestConstructorWhenNoFileIsAvailable)
{
    PersistentCognitoIdentityProvider_JsonFileImpl identityProvider("identityPoolId", "accountId");
    ASSERT_FALSE(identityProvider.HasIdentityId());
    ASSERT_FALSE(identityProvider.HasLogins());
    ASSERT_EQ("identityPoolId", identityProvider.GetIdentityPoolId());
    ASSERT_EQ("accountId", identityProvider.GetAccountId());

    Aws::String filePath = ComputeIdentityFilePath();
    std::ifstream shouldNotExist(filePath);
    ASSERT_FALSE(shouldNotExist.good());
}

TEST(PersistentCognitoIdentityProvider_JsonImpl_Test, TestConstructorWhenFileIsAvaiable)
{
    JsonValue theIdentityPoolWeWant;
    theIdentityPoolWeWant.WithString("IdentityId", "TheIdentityWeWant");

    JsonValue logins;
    logins.WithString("TestLoginName", "TestLoginValue");
    theIdentityPoolWeWant.WithObject("Logins", logins);

    JsonValue someOtherIdentityPool;
    someOtherIdentityPool.WithString("IdentityId", "SomeOtherIdentity");

    JsonValue identityDoc;
    identityDoc.WithObject("IdentityPoolWeWant", theIdentityPoolWeWant);
    identityDoc.WithObject("SomeOtherIdentityPool", someOtherIdentityPool);

    FileSystemUtils::CreateDirectoryIfNotExists(ComputeAwsDirPath().c_str());

    Aws::String filePath = ComputeIdentityFilePath();
    std::ofstream identityFile(filePath);
    identityFile << identityDoc.WriteReadable();
    identityFile.close();

    PersistentCognitoIdentityProvider_JsonFileImpl identityProvider("IdentityPoolWeWant", "accountId");
    FileSystemUtils::RemoveFileIfExists(filePath.c_str());

    ASSERT_TRUE(identityProvider.HasIdentityId());
    ASSERT_EQ(theIdentityPoolWeWant.GetString("IdentityId"), identityProvider.GetIdentityId());
    ASSERT_TRUE(identityProvider.HasLogins());
    ASSERT_EQ(1u, identityProvider.GetLogins().size());
    ASSERT_EQ("TestLoginName", identityProvider.GetLogins().begin()->first);
    ASSERT_EQ("TestLoginValue", identityProvider.GetLogins().begin()->second);
}

TEST(PersistentCognitoIdentityProvider_JsonImpl_Test, TestPersistance)
{
    JsonValue someOtherIdentityPool;
    someOtherIdentityPool.WithString("IdentityId", "SomeOtherIdentity");

    JsonValue identityDoc;
    identityDoc.WithObject("SomeOtherIdentityPool", someOtherIdentityPool);

    FileSystemUtils::CreateDirectoryIfNotExists(ComputeAwsDirPath().c_str());

    Aws::String filePath = ComputeIdentityFilePath();
    std::ofstream identityFile(filePath);
    identityFile << identityDoc.WriteReadable();
    identityFile.close();

    PersistentCognitoIdentityProvider_JsonFileImpl identityProvider("IdentityPoolWeWant", "accountId");

    EXPECT_FALSE(identityProvider.HasIdentityId());
    EXPECT_FALSE(identityProvider.HasLogins());

    identityProvider.PersistIdentityId("IdentityWeWant");
    Aws::Map<Aws::String, Aws::String> loginsMap;
    loginsMap["LoginName"] = "LoginValue";
    identityProvider.PersistLogins(loginsMap);

    EXPECT_EQ("IdentityWeWant", identityProvider.GetIdentityId());
    EXPECT_EQ("LoginName", identityProvider.GetLogins().begin()->first);
    EXPECT_EQ("LoginValue", identityProvider.GetLogins().begin()->second);

    std::ifstream identityFileInput(filePath);
    JsonValue finalIdentityDoc(identityFileInput);
    identityFileInput.close();

    FileSystemUtils::RemoveFileIfExists(filePath.c_str());
    ASSERT_TRUE(finalIdentityDoc.ValueExists("SomeOtherIdentityPool"));
    ASSERT_TRUE(finalIdentityDoc.ValueExists("IdentityPoolWeWant"));
    JsonValue ourIdentityPool = finalIdentityDoc.GetObject("IdentityPoolWeWant");
    ASSERT_EQ("IdentityWeWant", ourIdentityPool.GetString("IdentityId"));
    ASSERT_EQ("LoginName", ourIdentityPool.GetObject("Logins").GetAllObjects().begin()->first);
    ASSERT_EQ("LoginValue", ourIdentityPool.GetObject("Logins").GetAllObjects().begin()->second.AsString());
}
