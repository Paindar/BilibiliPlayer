#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>
#include "test_utils.h"

/**
 * @file ui_components_test.cpp
 * @brief Test fixtures and infrastructure for UI component testing
 * 
 * Provides:
 * - QtAppFixture: Ensures QApplication exists for Qt tests
 * - WidgetTestHelper: Common utilities for widget testing
 * - Mock signals for async UI operations
 * 
 * Phase 2 Task T013 - Establishes testing foundation for Phase 3 UI work
 */

/// @brief Qt application fixture - ensures QCoreApplication exists
/// Used for all tests requiring Qt signal/slot infrastructure
struct QtAppFixture {
    QtAppFixture() {
        testutils::ensureQCoreApplication();
    }
    ~QtAppFixture() = default;
};

/// @brief Helper utilities for UI component testing
class WidgetTestHelper {
public:
    /// Create a simple test widget with common controls
    static QWidget* createTestWidget() {
        QWidget* widget = new QWidget();
        widget->setObjectName("testWidget");
        widget->setWindowTitle("Test Widget");
        
        // Create common UI elements
        QLineEdit* input = new QLineEdit(widget);
        input->setObjectName("testInput");
        input->setPlaceholderText("Test input");
        
        QPushButton* button = new QPushButton("Click Me", widget);
        button->setObjectName("testButton");
        
        QLabel* label = new QLabel("Test Label", widget);
        label->setObjectName("testLabel");
        
        return widget;
    }
    
    /// Create a QStackedWidget with multiple pages for testing
    static QStackedWidget* createTestStackedWidget() {
        QStackedWidget* stack = new QStackedWidget();
        
        // Add multiple pages
        QWidget* page1 = new QWidget();
        page1->setObjectName("page1");
        page1->setStyleSheet("background-color: red;");
        stack->addWidget(page1);
        
        QWidget* page2 = new QWidget();
        page2->setObjectName("page2");
        page2->setStyleSheet("background-color: blue;");
        stack->addWidget(page2);
        
        QWidget* page3 = new QWidget();
        page3->setObjectName("page3");
        page3->setStyleSheet("background-color: green;");
        stack->addWidget(page3);
        
        return stack;
    }
};

/// @brief Test signal tracking helper
/// Used to verify signals are emitted correctly during UI operations
class SignalSpy {
public:
    explicit SignalSpy(QObject* obj, const char* signal)
        : m_count(0)
    {
        connect(obj, signal, this, &SignalSpy::onSignal);
    }
    
    int count() const { return m_count; }
    void reset() { m_count = 0; }
    
private slots:
    void onSignal() { m_count++; }
    
private:
    int m_count;
};

// ==================== Test Cases ====================

TEST_CASE_METHOD(QtAppFixture, "Qt Application exists for tests", "[ui][core]") {
    REQUIRE(QApplication::instance() != nullptr);
}

TEST_CASE_METHOD(QtAppFixture, "Create test widget", "[ui][widget]") {
    QWidget* widget = WidgetTestHelper::createTestWidget();
    REQUIRE(widget != nullptr);
    REQUIRE(widget->objectName() == "testWidget");
    
    QLineEdit* input = widget->findChild<QLineEdit*>("testInput");
    REQUIRE(input != nullptr);
    
    QPushButton* button = widget->findChild<QPushButton*>("testButton");
    REQUIRE(button != nullptr);
    
    QLabel* label = widget->findChild<QLabel*>("testLabel");
    REQUIRE(label != nullptr);
    
    delete widget;
}

TEST_CASE_METHOD(QtAppFixture, "Create stacked widget", "[ui][stacked]") {
    QStackedWidget* stack = WidgetTestHelper::createTestStackedWidget();
    REQUIRE(stack != nullptr);
    REQUIRE(stack->count() == 3);
    
    QWidget* page1 = stack->widget(0);
    REQUIRE(page1 != nullptr);
    REQUIRE(page1->objectName() == "page1");
    
    delete stack;
}

TEST_CASE_METHOD(QtAppFixture, "Switch stacked widget pages", "[ui][stacked]") {
    QStackedWidget* stack = WidgetTestHelper::createTestStackedWidget();
    
    // Test page switching
    stack->setCurrentIndex(0);
    REQUIRE(stack->currentIndex() == 0);
    
    stack->setCurrentIndex(1);
    REQUIRE(stack->currentIndex() == 1);
    
    stack->setCurrentIndex(2);
    REQUIRE(stack->currentIndex() == 2);
    
    delete stack;
}

TEST_CASE_METHOD(QtAppFixture, "Signal spy counts emissions", "[ui][signal]") {
    QPushButton* button = new QPushButton("Test");
    SignalSpy spy(button, SIGNAL(clicked()));
    
    REQUIRE(spy.count() == 0);
    
    button->click();
    REQUIRE(spy.count() == 1);
    
    button->click();
    REQUIRE(spy.count() == 2);
    
    spy.reset();
    REQUIRE(spy.count() == 0);
    
    delete button;
}

TEST_CASE_METHOD(QtAppFixture, "Find child widgets by object name", "[ui][widget]") {
    QWidget* parent = WidgetTestHelper::createTestWidget();
    
    // Test finding children
    QLineEdit* input = parent->findChild<QLineEdit*>("testInput");
    REQUIRE(input != nullptr);
    REQUIRE(input->placeholderText() == "Test input");
    
    QPushButton* button = parent->findChild<QPushButton*>("testButton");
    REQUIRE(button != nullptr);
    REQUIRE(button->text() == "Click Me");
    
    QLabel* label = parent->findChild<QLabel*>("testLabel");
    REQUIRE(label != nullptr);
    REQUIRE(label->text() == "Test Label");
    
    delete parent;
}
