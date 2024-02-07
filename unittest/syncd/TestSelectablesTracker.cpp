#include <gtest/gtest.h>

#include "SelectablesTracker.h"

using namespace syncd;

class SelectableEventHandlerTestHelper : public SelectableEventHandler
{
    public:

        SelectableEventHandlerTestHelper() {}
        ~SelectableEventHandlerTestHelper() {}

        void handleSelectableEvent() {}
};

class SelectablesTrackerTest : public ::testing::Test
{
    public:

        SelectablesTrackerTest() {}

        SelectablesTracker m_selectablesTracker_;
};

TEST_F(SelectablesTrackerTest, AddSelectableFailsIfSelectableNull)
{
    SelectableEventHandlerTestHelper eventHandler;

    EXPECT_FALSE(m_selectablesTracker_.addSelectableToTracker(
            /*selectable=*/nullptr, (SelectableEventHandler *)&eventHandler));
}

TEST_F(SelectablesTrackerTest, AddSelectableFailsIfEventHandlerNull)
{
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});

    EXPECT_FALSE(m_selectablesTracker_.addSelectableToTracker(
            (swss::Selectable *)&timer, /*eventHandler=*/nullptr));
}

TEST_F(SelectablesTrackerTest, AddSelectableSucceeds)
{
      swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
      SelectableEventHandlerTestHelper eventHandler;

      EXPECT_TRUE(m_selectablesTracker_.addSelectableToTracker(
              (swss::Selectable *)&timer, (SelectableEventHandler *)&eventHandler));
}

TEST_F(SelectablesTrackerTest, AddSelectableFailsIfSelectableExists)
{
    // Add a selectable timer to tracker.
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
    SelectableEventHandlerTestHelper eventHandler;

    EXPECT_TRUE(m_selectablesTracker_.addSelectableToTracker(
            (swss::Selectable *)&timer, (SelectableEventHandler *)&eventHandler));

    // Adding the same selectable timer to tracker should fail.
    SelectableEventHandlerTestHelper eventHandler2;
    EXPECT_FALSE(m_selectablesTracker_.addSelectableToTracker(
            (swss::Selectable *)&timer, (SelectableEventHandler *)&eventHandler2));
}

TEST_F(SelectablesTrackerTest, RemoveSelectableFailsIfSelectableNull)
{
    EXPECT_FALSE(
            m_selectablesTracker_.removeSelectableFromTracker(/*selectable=*/nullptr));
}

TEST_F(SelectablesTrackerTest, RemoveSelectableFailsIfSelectableNotExist)
{
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});

    EXPECT_FALSE(m_selectablesTracker_.removeSelectableFromTracker(
            (swss::Selectable *)&timer));
}

TEST_F(SelectablesTrackerTest, RemoveSelectableSucceeds)
{
    // Add a selectable timer.
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
    SelectableEventHandlerTestHelper eventHandler;

    EXPECT_TRUE(m_selectablesTracker_.addSelectableToTracker(
            (swss::Selectable *)&timer, (SelectableEventHandler *)&eventHandler));

    // Remove the selectable timer just added in tracker.
    EXPECT_TRUE(m_selectablesTracker_.removeSelectableFromTracker(
            (swss::Selectable *)&timer));

    // Removing the selectable timer again should fail.
    EXPECT_FALSE(m_selectablesTracker_.removeSelectableFromTracker(
            (swss::Selectable *)&timer));
}

TEST_F(SelectablesTrackerTest, NullptrSelectableIsNotTracked)
{
    EXPECT_FALSE(
            m_selectablesTracker_.selectableIsTracked(/*selectable=*/nullptr));
}

TEST_F(SelectablesTrackerTest, SelectableIsNotTracked)
{
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
    EXPECT_FALSE(
            m_selectablesTracker_.selectableIsTracked((swss::Selectable *)&timer));
}

TEST_F(SelectablesTrackerTest, SelectableIsTracked)
{
    // Add the Selectable in the tracker.
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
    SelectableEventHandlerTestHelper eventHandler;

    EXPECT_TRUE(m_selectablesTracker_.addSelectableToTracker(
            (swss::Selectable *)&timer, (SelectableEventHandler *)&eventHandler));

    // Verify the Selectable in the tracker.
    EXPECT_TRUE(
        m_selectablesTracker_.selectableIsTracked((swss::Selectable *)&timer));

    // Remove the Selectable from tracker.
    EXPECT_TRUE(m_selectablesTracker_.removeSelectableFromTracker(
        (swss::Selectable *)&timer));

    // Check the Selectable in tracker again, this time it should fail.
    EXPECT_FALSE(
        m_selectablesTracker_.selectableIsTracked((swss::Selectable *)&timer));
}

TEST_F(SelectablesTrackerTest, GetEventHandlerFailsIfSelectableNull)
{
    EXPECT_EQ(
            m_selectablesTracker_.getEventHandlerForSelectable(/*selectable=*/nullptr),
            nullptr);
}

TEST_F(SelectablesTrackerTest, GetEventHandlerFailsIfSelectableNotExist)
{
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
    EXPECT_EQ(m_selectablesTracker_.getEventHandlerForSelectable(
                  (swss::Selectable *)&timer),
              nullptr);
}

TEST_F(SelectablesTrackerTest, GetEventHandlerSucceeds)
{
    // Add a selectable timer.
    swss::SelectableTimer timer(/*interval=*/{.tv_sec = 10, .tv_nsec = 20});
    SelectableEventHandlerTestHelper eventHandler;

    EXPECT_TRUE(m_selectablesTracker_.addSelectableToTracker(
            (swss::Selectable *)&timer, (SelectableEventHandler *)&eventHandler));

    // Get the event handler for the selectable timer.
    EXPECT_EQ(m_selectablesTracker_.getEventHandlerForSelectable(
                  (swss::Selectable *)&timer),
              (SelectableEventHandler *)&eventHandler);
}
