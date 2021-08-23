#include "OidRefCounter.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(OidRefCounter, objectReferenceIncrement)
{
    OidRefCounter c;

    EXPECT_THROW(c.objectReferenceIncrement(1), std::runtime_error);
}

TEST(OidRefCounter, objectReferenceDecrement)
{
    OidRefCounter c;

    EXPECT_THROW(c.objectReferenceDecrement(1), std::runtime_error);

    c.objectReferenceInsert(2);

    EXPECT_THROW(c.objectReferenceDecrement(2), std::runtime_error);
}

TEST(OidRefCounter, objectReferenceDecrement_list)
{
    OidRefCounter c;

    sai_object_list_t list;

    sai_object_id_t l[2] = {1, 2};

    list.count = 2;
    list.list = l;

    EXPECT_THROW(c.objectReferenceDecrement(list), std::runtime_error);
}

TEST(OidRefCounter, objectReferenceInsert)
{
    OidRefCounter c;

    c.objectReferenceInsert(2);

    EXPECT_THROW(c.objectReferenceInsert(2), std::runtime_error);
}

TEST(OidRefCounter, objectReferenceRemove)
{
    OidRefCounter c;

    c.objectReferenceInsert(2);

    c.objectReferenceIncrement(2);

    EXPECT_THROW(c.objectReferenceRemove(2), std::runtime_error);

    EXPECT_THROW(c.objectReferenceRemove(3), std::runtime_error);
}

TEST(OidRefCounter, getObjectReferenceCount)
{
    OidRefCounter c;

    EXPECT_THROW(c.getObjectReferenceCount(3), std::runtime_error);
}


TEST(OidRefCounter, isObjectInUse)
{
    OidRefCounter c;

    EXPECT_THROW(c.isObjectInUse(3), std::runtime_error);
}

TEST(OidRefCounter, getAllReferences)
{
    OidRefCounter c;

    EXPECT_EQ(c.getAllReferences().size(), 0);
}

TEST(OidRefCounter, objectReferenceClear)
{
    OidRefCounter c;

    EXPECT_THROW(c.objectReferenceClear(2), std::runtime_error);
}
