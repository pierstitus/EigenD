/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

#include "JuceDemoHeader.h"

//==============================================================================
struct AlphabeticDemoSorter
{
    static int compareElements (const JuceDemoTypeBase* first, const JuceDemoTypeBase* second)
    {
        return first->name.compare (second->name);
    }
};

JuceDemoTypeBase::JuceDemoTypeBase (const String& demoName)  : name (demoName)
{
    AlphabeticDemoSorter sorter;
    getDemoTypeList().addSorted (sorter, this);
}

JuceDemoTypeBase::~JuceDemoTypeBase()
{
    getDemoTypeList().removeFirstMatchingValue (this);
}

Array<JuceDemoTypeBase*>& JuceDemoTypeBase::getDemoTypeList()
{
    static Array<JuceDemoTypeBase*> list;
    return list;
}

//==============================================================================
#if JUCE_WINDOWS || JUCE_LINUX || JUCE_MAC

// Just add a simple icon to the Window system tray area or Mac menu bar..
class DemoTaskbarComponent  : public SystemTrayIconComponent,
                              private Timer
{
public:
    DemoTaskbarComponent()
    {
        setIconImage (ImageCache::getFromMemory (BinaryData::juce_icon_png, BinaryData::juce_icon_pngSize));
        setIconTooltip ("Juce Demo App!");
    }

    void mouseDown (const MouseEvent&) override
    {
        // On OSX, there can be problems launching a menu when we're not the foreground
        // process, so just in case, we'll first make our process active, and then use a
        // timer to wait a moment before opening our menu, which gives the OS some time to
        // get its act together and bring our windows to the front.

        Process::makeForegroundProcess();
        startTimer (50);
    }

    // This is invoked when the menu is clicked or dismissed
    static void menuInvocationCallback (int chosenItemID, DemoTaskbarComponent*)
    {
        if (chosenItemID == 1)
            JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    void timerCallback() override
    {
        stopTimer();

        PopupMenu m;
        m.addItem (1, "Quit the Juce demo");

        // It's always better to open menus asynchronously when possible.
        m.showMenuAsync (PopupMenu::Options(),
                         ModalCallbackFunction::forComponent (menuInvocationCallback, this));
    }
};

#endif

bool juceDemoRepaintDebuggingActive = false;

//==============================================================================
class ContentComponent   : public Component,
                           public ListBoxModel,
                           public ApplicationCommandTarget
{
public:
    ContentComponent()
    {
        LookAndFeel::setDefaultLookAndFeel (&lookAndFeelV3);
        addAndMakeVisible (demoList);

        demoList.setModel (this);
        demoList.setColour (ListBox::backgroundColourId, Colour::greyLevel (0.7f));
        demoList.selectRow (0);
    }

    void resized() override
    {
        Rectangle<int> r (getLocalBounds());

        if (r.getWidth() > 600)
        {
            demoList.setBounds (r.removeFromLeft (210));
            demoList.setRowHeight (20);
        }
        else
        {
            demoList.setBounds (r.removeFromLeft (130));
            demoList.setRowHeight (30);
        }

        if (currentDemo != nullptr)
            currentDemo->setBounds (r);
    }

    int getNumRows() override
    {
        return JuceDemoTypeBase::getDemoTypeList().size();
    }

    void paintListBoxItem (int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll (findColour (TextEditor::highlightColourId));

        if (JuceDemoTypeBase* type = JuceDemoTypeBase::getDemoTypeList() [rowNumber])
        {
            String name (type->name.trimCharactersAtStart ("0123456789").trimStart());

            AttributedString a;
            a.setJustification (Justification::centredLeft);

            String category;

            if (name.containsChar (':'))
            {
                category = name.upToFirstOccurrenceOf (":", true, false);
                name = name.fromFirstOccurrenceOf (":", false, false).trim();

                if (height > 20)
                    category << "\n";
                else
                    category << " ";
            }

            if (category.isNotEmpty())
                a.append (category, Font (10.0f), Colours::black);

            a.append (name, Font (13.0f).boldened(), Colours::black);

            a.draw (g, Rectangle<int> (width + 10, height).reduced (6, 0).toFloat());
        }
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (JuceDemoTypeBase* selectedDemoType = JuceDemoTypeBase::getDemoTypeList() [lastRowSelected])
        {
            currentDemo = nullptr;
            addAndMakeVisible (currentDemo = selectedDemoType->createComponent());
            currentDemo->setName (selectedDemoType->name);
            resized();
        }
    }

    MouseCursor getMouseCursorForRow (int /*row*/) override
    {
        return MouseCursor::PointingHandCursor;
    }

    int getCurrentPageIndex() const noexcept
    {
        if (currentDemo == nullptr)
            return -1;

        Array<JuceDemoTypeBase*>& demos (JuceDemoTypeBase::getDemoTypeList());

        for (int i = demos.size(); --i >= 0;)
            if (demos.getUnchecked (i)->name == currentDemo->getName())
                return i;

        return -1;
    }

    void moveDemoPages (int numPagesToMove)
    {
        const int newIndex = negativeAwareModulo (getCurrentPageIndex() + numPagesToMove,
                                                  JuceDemoTypeBase::getDemoTypeList().size());
        demoList.selectRow (newIndex);
        // we have to go through our demo list here or it won't be updated to reflect the current demo
    }

    bool isShowingOpenGLDemo() const
    {
        return currentDemo != nullptr && currentDemo->getName().contains ("OpenGL");
    }

private:
    ListBox demoList;
    ScopedPointer<Component> currentDemo;

    LookAndFeel_V1 lookAndFeelV1;
    LookAndFeel_V2 lookAndFeelV2;
    LookAndFeel_V3 lookAndFeelV3;

    //==============================================================================
    // The following methods implement the ApplicationCommandTarget interface, allowing
    // this window to publish a set of actions it can perform, and which can be mapped
    // onto menus, keypresses, etc.

    ApplicationCommandTarget* getNextCommandTarget() override
    {
        // this will return the next parent component that is an ApplicationCommandTarget (in this
        // case, there probably isn't one, but it's best to use this method in your own apps).
        return findFirstTargetParentComponent();
    }

    void getAllCommands (Array<CommandID>& commands) override
    {
        // this returns the set of all commands that this target can perform..
        const CommandID ids[] = { MainAppWindow::showPreviousDemo,
                                  MainAppWindow::showNextDemo,
                                  MainAppWindow::welcome,
                                  MainAppWindow::componentsAnimation,
                                  MainAppWindow::componentsDialogBoxes,
                                  MainAppWindow::componentsKeyMappings,
                                  MainAppWindow::componentsMDI,
                                  MainAppWindow::componentsPropertyEditors,
                                  MainAppWindow::componentsTransforms,
                                  MainAppWindow::componentsWebBrowsers,
                                  MainAppWindow::componentsWidgets,
                                  MainAppWindow::useLookAndFeelV1,
                                  MainAppWindow::useLookAndFeelV2,
                                  MainAppWindow::useLookAndFeelV3,
                                  MainAppWindow::toggleRepaintDebugging,
                                 #if ! JUCE_LINUX
                                  MainAppWindow::goToKioskMode,
                                 #endif
                                  MainAppWindow::useNativeTitleBar
                                };

        commands.addArray (ids, numElementsInArray (ids));

        const CommandID engineIDs[] = { MainAppWindow::renderingEngineOne,
                                        MainAppWindow::renderingEngineTwo,
                                        MainAppWindow::renderingEngineThree };

        StringArray renderingEngines (MainAppWindow::getMainAppWindow()->getRenderingEngines());
        commands.addArray (engineIDs, renderingEngines.size());
    }

    void getCommandInfo (CommandID commandID, ApplicationCommandInfo& result) override
    {
        const String generalCategory ("General");
        const String demosCategory ("Demos");

        switch (commandID)
        {
            case MainAppWindow::showPreviousDemo:
                result.setInfo ("Previous Demo", "Shows the previous demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('-', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::showNextDemo:
                result.setInfo ("Next Demo", "Shows the next demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('=', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::welcome:
                result.setInfo ("Welcome Demo", "Shows the first demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('1', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsAnimation:
                result.setInfo ("Animation Demo", "Shows the second demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('2', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsDialogBoxes:
                result.setInfo ("Dialog Boxes Demo", "Shows the third demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('3', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsKeyMappings:
                result.setInfo ("Key Mappings Demo", "Shows the fourth demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('4', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsMDI:
                result.setInfo ("Multi-Document Demo", "Shows the fith demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('5', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsPropertyEditors:
                result.setInfo ("Property Editor Demo", "Shows the sixth demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('6', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsTransforms:
                result.setInfo ("Component Transforms Demo", "Shows the sevent demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('7', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsWebBrowsers:
                result.setInfo ("Web Browser Demo", "Shows the eight demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('8', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::componentsWidgets:
                result.setInfo ("Widgets Demo", "Shows the ninth demo in the list", demosCategory, 0);
                result.addDefaultKeypress ('9', ModifierKeys::commandModifier);
                break;

            case MainAppWindow::renderingEngineOne:
            case MainAppWindow::renderingEngineTwo:
            case MainAppWindow::renderingEngineThree:
            {
                MainAppWindow& mainWindow = *MainAppWindow::getMainAppWindow();
                const StringArray engines (mainWindow.getRenderingEngines());
                const int index = commandID - MainAppWindow::renderingEngineOne;

                result.setInfo ("Use " + engines[index], "Uses the " + engines[index] + " engine to render the UI", generalCategory, 0);
                result.setTicked (mainWindow.getActiveRenderingEngine() == index);

                result.addDefaultKeypress ('1' + index, ModifierKeys::noModifiers);
                break;
            }

            case MainAppWindow::useLookAndFeelV1:
                result.setInfo ("Use LookAndFeel_V1", String::empty, generalCategory, 0);
                result.addDefaultKeypress ('i', ModifierKeys::commandModifier);
                result.setTicked (typeid (LookAndFeel_V1) == typeid (getLookAndFeel()));
                break;

            case MainAppWindow::useLookAndFeelV2:
                result.setInfo ("Use LookAndFeel_V2", String::empty, generalCategory, 0);
                result.addDefaultKeypress ('o', ModifierKeys::commandModifier);
                result.setTicked (typeid (LookAndFeel_V2) == typeid (getLookAndFeel()));
                break;

            case MainAppWindow::useLookAndFeelV3:
                result.setInfo ("Use LookAndFeel_V3", String::empty, generalCategory, 0);
                result.addDefaultKeypress ('p', ModifierKeys::commandModifier);
                result.setTicked (typeid (LookAndFeel_V3) == typeid (getLookAndFeel()));
                break;

            case MainAppWindow::toggleRepaintDebugging:
                result.setInfo ("Toggle repaint display", String::empty, generalCategory, 0);
                result.addDefaultKeypress ('r', ModifierKeys());
                result.setTicked (juceDemoRepaintDebuggingActive);
                break;

            case MainAppWindow::useNativeTitleBar:
            {
                result.setInfo ("Use native window title bar", String::empty, generalCategory, 0);
                result.addDefaultKeypress ('n', ModifierKeys::commandModifier);
                bool nativeTitlebar = false;

                if (MainAppWindow* map = MainAppWindow::getMainAppWindow())
                    nativeTitlebar = map->isUsingNativeTitleBar();

                result.setTicked (nativeTitlebar);
                break;
            }

           #if ! JUCE_LINUX
            case MainAppWindow::goToKioskMode:
                result.setInfo ("Show full-screen kiosk mode", String::empty, generalCategory, 0);
                result.addDefaultKeypress ('f', ModifierKeys::commandModifier);
                result.setTicked (Desktop::getInstance().getKioskModeComponent() != 0);
                break;
           #endif

            default:
                break;
        }
    }

    bool perform (const InvocationInfo& info) override
    {
        MainAppWindow* mainWindow = MainAppWindow::getMainAppWindow();

        if (mainWindow == nullptr)
            return true;

        switch (info.commandID)
        {
            case MainAppWindow::showPreviousDemo:   moveDemoPages (-1); break;
            case MainAppWindow::showNextDemo:       moveDemoPages ( 1); break;

            case MainAppWindow::welcome:
            case MainAppWindow::componentsAnimation:
            case MainAppWindow::componentsDialogBoxes:
            case MainAppWindow::componentsKeyMappings:
            case MainAppWindow::componentsMDI:
            case MainAppWindow::componentsPropertyEditors:
            case MainAppWindow::componentsTransforms:
            case MainAppWindow::componentsWebBrowsers:
            case MainAppWindow::componentsWidgets:
                demoList.selectRow (info.commandID - MainAppWindow::welcome);
                break;

            case MainAppWindow::renderingEngineOne:
            case MainAppWindow::renderingEngineTwo:
            case MainAppWindow::renderingEngineThree:
                mainWindow->setRenderingEngine (info.commandID - MainAppWindow::renderingEngineOne);
                break;

            case MainAppWindow::useLookAndFeelV1:  LookAndFeel::setDefaultLookAndFeel (&lookAndFeelV1); break;
            case MainAppWindow::useLookAndFeelV2:  LookAndFeel::setDefaultLookAndFeel (&lookAndFeelV2); break;
            case MainAppWindow::useLookAndFeelV3:  LookAndFeel::setDefaultLookAndFeel (&lookAndFeelV3); break;

            case MainAppWindow::toggleRepaintDebugging:
                juceDemoRepaintDebuggingActive = ! juceDemoRepaintDebuggingActive;
                mainWindow->repaint();
                break;

            case MainAppWindow::useNativeTitleBar:
                mainWindow->setUsingNativeTitleBar (! mainWindow->isUsingNativeTitleBar());
                break;

           #if ! JUCE_LINUX
            case MainAppWindow::goToKioskMode:
                {
                    Desktop& desktop = Desktop::getInstance();

                    if (desktop.getKioskModeComponent() == nullptr)
                        desktop.setKioskModeComponent (getTopLevelComponent());
                    else
                        desktop.setKioskModeComponent (nullptr);

                    break;
                }
           #endif

            default:
                return false;
        }

        return true;
    }
};

//==============================================================================
static ScopedPointer<ApplicationCommandManager> applicationCommandManager;
static ScopedPointer<AudioDeviceManager> sharedAudioDeviceManager;

MainAppWindow::MainAppWindow()
    : DocumentWindow (JUCEApplication::getInstance()->getApplicationName(),
                      Colours::lightgrey,
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (400, 400, 10000, 10000);

   #if JUCE_IOS || JUCE_ANDROID
    setFullScreen (true);
   #else
    setBounds ((int) (0.1f * getParentWidth()),
               (int) (0.1f * getParentHeight()),
               jmax (850, (int) (0.5f * getParentWidth())),
               jmax (600, (int) (0.7f * getParentHeight())));
   #endif

    contentComponent = new ContentComponent();
    setContentNonOwned (contentComponent, false);
    setVisible (true);

    // this lets the command manager use keypresses that arrive in our window to send out commands
    addKeyListener (getApplicationCommandManager().getKeyMappings());

   #if JUCE_WINDOWS || JUCE_LINUX || JUCE_MAC
    taskbarIcon = new DemoTaskbarComponent();
   #endif

    triggerAsyncUpdate();
}

MainAppWindow::~MainAppWindow()
{
    clearContentComponent();
    contentComponent = nullptr;
    applicationCommandManager = nullptr;
    sharedAudioDeviceManager = nullptr;

   #if JUCE_OPENGL
    openGLContext.detach();
   #endif
}

void MainAppWindow::closeButtonPressed()
{
    JUCEApplication::getInstance()->systemRequestedQuit();
}

ApplicationCommandManager& MainAppWindow::getApplicationCommandManager()
{
    if (applicationCommandManager == nullptr)
        applicationCommandManager = new ApplicationCommandManager();

    return *applicationCommandManager;
}

AudioDeviceManager& MainAppWindow::getSharedAudioDeviceManager()
{
    if (sharedAudioDeviceManager == nullptr)
    {
        sharedAudioDeviceManager = new AudioDeviceManager();
        sharedAudioDeviceManager->initialise (2, 2, 0, true, String::empty, 0);
    }

    return *sharedAudioDeviceManager;
}

MainAppWindow* MainAppWindow::getMainAppWindow()
{
    for (int i = TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
        if (MainAppWindow* maw = dynamic_cast<MainAppWindow*> (TopLevelWindow::getTopLevelWindow (i)))
            return maw;

    return nullptr;
}

void MainAppWindow::handleAsyncUpdate()
{
    // This registers all of our commands with the command manager but has to be done after the window has
    // been created so we can find the number of rendering engines available
    ApplicationCommandManager& commandManager = MainAppWindow::getApplicationCommandManager();
    commandManager.registerAllCommandsForTarget (contentComponent);
    commandManager.registerAllCommandsForTarget (JUCEApplication::getInstance());
}

void MainAppWindow::showMessageBubble (const String& text)
{
    BubbleMessageComponent* bm = new BubbleMessageComponent (500);
    getContentComponent()->addChildComponent (bm);

    AttributedString attString;
    attString.append (text, Font (15.0f));

    bm->showAt (Rectangle<int> (getLocalBounds().getCentreX(), 10, 1, 1),
                attString,
                500,  // numMillisecondsBeforeRemoving
                true,  // removeWhenMouseClicked
                true); // deleteSelfAfterUse
}

static const char* openGLRendererName = "OpenGL Renderer";

StringArray MainAppWindow::getRenderingEngines() const
{
    StringArray renderingEngines;

    if (ComponentPeer* peer = getPeer())
        renderingEngines = peer->getAvailableRenderingEngines();

   #if JUCE_OPENGL
    renderingEngines.add (openGLRendererName);
   #endif

    return renderingEngines;
}

void MainAppWindow::setRenderingEngine (int index)
{
    showMessageBubble (getRenderingEngines()[index]);

   #if JUCE_OPENGL
    if (getRenderingEngines()[index] == openGLRendererName
          && contentComponent != nullptr
          && (! contentComponent->isShowingOpenGLDemo()))
    {
        openGLContext.attachTo (*getTopLevelComponent());
        return;
    }

    openGLContext.detach();
   #endif

    if (ComponentPeer* peer = getPeer())
        peer->setCurrentRenderingEngine (index);
}

int MainAppWindow::getActiveRenderingEngine() const
{
   #if JUCE_OPENGL
    if (openGLContext.isAttached())
        return getRenderingEngines().indexOf (openGLRendererName);
   #endif

    if (ComponentPeer* peer = getPeer())
        return peer->getCurrentRenderingEngine();

    return 0;
}

Path MainAppWindow::getJUCELogoPath()
{
    // This data was creating using Path::writePathToStream()

    static const unsigned char mainJuceLogo[] = { 110,109,104,98,101,67,226,177,185,67,98,216,179,86,67,250,116,179,67,120,184,74,67,126,86,174,67,24,194,74,67,178,81,174,67,98,152,203,74,67,230,76,174,67,216,11,78,67,202,111,174,67,88,251,81,67,70,159,174,67,98,216,234,85,67,186,206,174,67,136,52,89,
        67,170,242,174,67,216,73,89,67,34,239,174,67,98,24,95,89,67,150,235,174,67,40,132,85,67,46,112,173,67,168,184,80,67,2,164,171,67,108,184,0,72,67,90,95,168,67,108,248,166,79,67,214,89,168,67,108,56,77,87,67,86,84,168,67,108,184,54,83,67,182,151,166,67,
        98,56,247,80,67,42,163,165,67,152,5,79,67,18,219,164,67,8,229,78,67,18,219,164,67,98,120,196,78,67,18,219,164,67,24,42,78,67,118,233,164,67,8,142,77,67,10,251,164,67,98,248,241,76,67,154,12,165,67,200,163,75,67,102,40,165,67,88,167,74,67,206,56,165,67,
        98,232,170,73,67,54,73,165,67,120,215,72,67,210,89,165,67,120,209,72,67,178,93,165,67,98,104,203,72,67,138,97,165,67,168,220,71,67,130,95,167,67,200,190,70,67,214,202,169,67,108,248,182,68,67,238,48,174,67,108,72,122,69,67,130,142,174,67,98,72,50,70,
        67,166,230,174,67,152,61,70,67,150,242,174,67,152,61,70,67,30,92,175,67,98,152,61,70,67,134,193,175,67,104,46,70,67,194,211,175,67,56,157,69,67,94,28,176,67,98,24,230,67,67,238,247,176,67,88,241,64,67,150,131,176,67,24,242,64,67,166,100,175,67,98,24,
        242,64,67,206,228,174,67,104,3,66,67,54,90,174,67,120,56,67,67,2,61,174,67,98,72,169,67,67,86,50,174,67,24,194,67,67,46,38,174,67,24,194,67,67,154,249,173,67,98,24,194,67,67,174,219,173,67,120,231,67,67,146,132,173,67,24,21,68,67,6,56,173,67,98,200,66,
        68,67,118,235,172,67,72,31,69,67,182,23,171,67,40,255,69,67,146,40,169,67,98,248,222,70,67,106,57,167,67,168,159,71,67,162,147,165,67,88,171,71,67,62,127,165,67,98,184,190,71,67,82,93,165,67,104,146,71,67,38,90,165,67,152,161,69,67,254,89,165,67,98,248,
        118,68,67,254,89,165,67,24,217,66,67,30,80,165,67,232,9,66,67,62,68,165,67,98,200,58,65,67,98,56,165,67,24,139,64,67,202,49,165,67,120,131,64,67,154,53,165,67,98,184,123,64,67,114,57,165,67,136,174,62,67,22,20,169,67,72,130,60,67,58,198,173,67,98,232,
        10,57,67,234,67,181,67,136,151,56,67,74,81,182,67,104,213,56,67,26,91,182,67,98,24,252,56,67,62,97,182,67,136,81,57,67,82,130,182,67,40,147,57,67,154,164,182,67,98,8,61,58,67,58,253,182,67,136,86,58,67,30,146,183,67,56,205,57,67,34,247,183,67,98,184,
        182,56,67,246,195,184,67,24,38,54,67,210,204,184,67,168,45,53,67,34,7,184,67,98,168,99,52,67,86,102,183,67,216,50,53,67,214,136,182,67,72,200,54,67,18,80,182,67,98,232,106,55,67,82,57,182,67,216,135,55,67,46,44,182,67,24,171,55,67,74,233,181,67,98,88,
        6,56,67,250,59,181,67,152,72,63,67,210,117,165,67,136,83,63,67,22,69,165,67,98,24,93,63,67,150,26,165,67,152,74,63,67,230,14,165,67,120,245,62,67,150,9,165,67,98,56,47,62,67,54,253,164,67,72,49,58,67,102,114,164,67,56,27,57,67,18,62,164,67,98,152,31,
        56,67,186,14,164,67,72,4,56,67,54,13,164,67,168,241,55,67,170,45,164,67,98,120,230,55,67,82,65,164,67,200,160,54,67,186,255,166,67,8,30,53,67,142,70,170,67,98,88,155,51,67,102,141,173,67,88,86,50,67,254,73,176,67,216,75,50,67,122,91,176,67,98,104,64,
        50,67,162,110,176,67,152,106,50,67,118,137,176,67,72,182,50,67,18,159,176,67,98,40,219,51,67,174,242,176,67,168,44,52,67,222,178,177,67,104,102,51,67,30,62,178,67,98,40,102,50,67,10,242,178,67,200,190,47,67,18,239,178,67,56,208,46,67,6,57,178,67,98,168,
        115,46,67,106,242,177,67,200,121,46,67,234,104,177,67,88,221,46,67,194,19,177,67,98,136,51,47,67,250,201,176,67,24,69,48,67,150,113,176,67,232,212,48,67,10,113,176,67,98,184,27,49,67,10,113,176,67,232,56,49,67,62,93,176,67,200,91,49,67,170,22,176,67,
        98,8,251,49,67,10,213,174,67,88,6,55,67,170,215,163,67,216,251,54,67,58,213,163,67,98,200,244,54,67,146,211,163,67,72,51,54,67,158,164,163,67,248,77,53,67,214,108,163,67,98,88,86,52,67,158,48,163,67,248,141,51,67,102,12,163,67,120,96,51,67,158,19,163,
        67,98,248,249,50,67,226,35,163,67,216,71,49,67,46,1,163,67,88,157,46,67,30,178,162,67,108,88,107,44,67,6,113,162,67,108,184,215,43,67,210,177,162,67,98,152,242,42,67,106,22,163,67,184,219,40,67,86,197,163,67,136,154,39,67,174,20,164,67,98,136,250,38,
        67,62,60,164,67,152,119,38,67,94,103,164,67,152,119,38,67,134,116,164,67,98,152,119,38,67,178,129,164,67,152,242,38,67,186,68,165,67,232,136,39,67,242,37,166,67,98,56,31,40,67,38,7,167,67,184,174,40,67,242,247,167,67,200,199,40,67,6,61,168,67,98,184,
        17,41,67,218,8,169,67,104,176,40,67,126,153,169,67,120,150,39,67,34,3,170,67,98,24,204,38,67,238,78,170,67,152,253,34,67,246,82,171,67,88,46,33,67,206,184,171,67,108,88,158,31,67,190,16,172,67,108,248,249,30,67,174,235,171,67,98,152,237,29,67,46,175,
        171,67,40,80,27,67,34,44,170,67,120,187,24,67,118,79,168,67,98,72,62,24,67,46,245,167,67,152,195,23,67,86,171,167,67,184,170,23,67,86,171,167,67,98,232,145,23,67,90,171,167,67,120,219,22,67,162,185,167,67,104,21,22,67,22,203,167,67,98,104,79,21,67,134,
        220,167,67,136,87,20,67,246,234,167,67,184,238,19,67,34,235,167,67,98,216,133,19,67,34,235,167,67,152,27,19,67,134,242,167,67,136,2,19,67,14,251,167,67,98,136,233,18,67,150,3,168,67,24,130,18,67,2,125,168,67,200,28,18,67,210,8,169,67,98,232,177,16,67,
        114,253,170,67,136,53,15,67,42,153,172,67,136,108,14,67,222,6,173,67,98,232,39,14,67,86,44,173,67,56,231,13,67,130,54,173,67,40,62,13,67,110,54,173,67,98,24,156,11,67,110,54,173,67,24,218,6,67,250,184,172,67,56,98,3,67,234,50,172,67,108,152,163,2,67,
        34,22,172,67,108,72,166,2,67,202,125,171,67,98,88,168,2,67,254,41,171,67,232,161,2,67,110,229,170,67,88,153,2,67,110,229,170,67,98,216,144,2,67,110,229,170,67,72,18,1,67,206,90,171,67,176,142,254,66,66,234,171,67,98,48,105,234,66,78,16,175,67,144,149,
        220,66,6,141,176,67,16,174,204,66,170,71,177,67,98,80,89,201,66,194,110,177,67,112,92,199,66,226,118,177,67,176,91,192,66,10,122,177,67,98,16,23,184,66,202,125,177,67,16,255,183,66,78,126,177,67,144,194,182,66,254,177,177,67,98,48,23,180,66,150,33,178,
        67,240,81,173,66,206,7,179,67,16,49,169,66,86,127,179,67,98,208,99,156,66,18,242,180,67,112,13,145,66,90,188,181,67,144,182,129,66,182,63,182,67,98,224,152,121,66,202,105,182,67,160,106,89,66,250,98,182,67,192,33,79,66,162,52,182,67,98,160,48,35,66,154,
        110,181,67,32,23,1,66,94,84,179,67,192,100,211,65,138,243,175,67,98,128,131,143,65,218,12,171,67,128,22,118,65,142,127,163,67,0,225,163,65,26,108,158,67,98,192,134,192,65,126,220,154,67,64,214,241,65,158,158,152,67,64,184,21,66,122,66,152,67,98,64,121,
        35,66,118,22,152,67,32,193,48,66,34,114,152,67,192,112,62,66,130,91,153,67,98,160,132,71,66,78,246,153,67,128,149,76,66,166,109,154,67,192,118,83,66,194,74,155,67,98,32,75,106,66,126,40,158,67,0,12,116,66,146,185,162,67,32,232,107,66,118,188,166,67,98,
        64,171,105,66,182,214,167,67,160,96,102,66,74,145,168,67,0,1,96,66,58,95,169,67,98,32,58,84,66,186,219,170,67,160,229,69,66,246,71,171,67,160,43,51,66,234,177,170,67,98,0,68,41,66,134,98,170,67,160,213,34,66,110,229,169,67,192,255,27,66,38,239,168,67,
        98,128,5,19,66,174,171,167,67,128,231,15,66,38,45,166,67,224,209,18,66,178,136,164,67,98,224,0,21,66,234,77,163,67,160,189,25,66,226,145,162,67,160,151,33,66,114,62,162,67,98,64,150,39,66,186,254,161,67,96,195,47,66,118,65,162,67,192,253,51,66,154,212,
        162,67,108,96,165,53,66,50,14,163,67,108,96,1,50,66,234,210,162,67,98,32,185,45,66,50,141,162,67,0,92,39,66,30,130,162,67,160,89,35,66,106,185,162,67,98,224,214,31,66,206,233,162,67,128,167,27,66,74,107,163,67,128,160,25,66,46,230,163,67,98,128,7,24,
        66,18,71,164,67,64,220,23,66,238,102,164,67,32,225,23,66,218,47,165,67,98,32,225,23,66,46,170,165,67,96,78,24,66,238,61,166,67,32,205,24,66,46,120,166,67,98,64,228,28,66,70,89,168,67,160,251,43,66,182,167,169,67,32,154,61,66,182,167,169,67,98,0,233,71,
        66,182,167,169,67,96,54,79,66,114,82,169,67,128,27,85,66,70,149,168,67,98,224,182,97,66,174,0,167,67,192,95,101,66,210,206,163,67,64,242,93,66,90,222,160,67,98,128,176,89,66,26,47,159,67,0,26,83,66,14,212,157,67,160,108,73,66,58,165,156,67,98,0,214,64,
        66,122,152,155,67,64,61,55,66,174,235,154,67,160,133,43,66,210,138,154,67,98,160,187,37,66,246,90,154,67,0,46,36,66,178,86,154,67,96,212,27,66,186,95,154,67,98,160,222,16,66,150,107,154,67,64,115,12,66,94,143,154,67,224,172,2,66,122,43,155,67,98,128,
        125,222,65,186,97,156,67,128,208,191,65,238,186,158,67,0,173,179,65,10,108,161,67,98,192,81,162,65,122,69,165,67,192,153,175,65,46,233,169,67,64,247,214,65,66,192,173,67,98,192,139,227,65,98,250,174,67,128,19,250,65,222,133,176,67,192,214,5,66,122,93,
        177,67,98,32,176,22,66,74,250,178,67,32,169,47,66,226,98,180,67,32,217,70,66,50,8,181,67,98,160,162,87,66,226,127,181,67,128,64,95,66,114,153,181,67,0,55,114,66,210,153,181,67,98,192,152,130,66,210,153,181,67,128,95,135,66,162,122,181,67,48,157,143,66,
        38,5,181,67,98,80,101,155,66,46,93,180,67,112,247,167,66,142,19,179,67,16,207,177,66,122,132,177,67,108,176,187,179,66,114,54,177,67,108,16,52,177,66,226,18,177,67,98,240,109,173,66,210,221,176,67,176,158,169,66,30,118,176,67,16,234,167,66,10,22,176,
        67,98,16,24,167,66,210,231,175,67,208,122,166,66,82,190,175,67,208,140,166,66,218,185,175,67,98,176,158,166,66,98,181,175,67,240,180,167,66,154,204,175,67,208,246,168,66,126,237,175,67,98,240,205,171,66,210,55,176,67,112,196,176,66,2,144,176,67,80,242,
        179,66,174,176,176,67,108,176,90,182,66,110,201,176,67,108,240,202,184,66,58,88,176,67,98,16,100,189,66,198,130,175,67,16,195,193,66,94,141,174,67,240,143,198,66,34,83,173,67,98,112,94,205,66,166,149,171,67,80,246,209,66,102,31,170,67,176,42,215,66,86,
        10,168,67,98,48,184,217,66,218,4,167,67,16,35,222,66,118,4,165,67,208,138,222,66,222,178,164,67,98,112,180,222,66,34,146,164,67,80,121,222,66,54,103,164,67,48,191,221,66,178,30,164,67,108,48,185,220,66,154,184,163,67,108,144,170,217,66,222,231,163,67,
        98,16,237,210,66,18,80,164,67,240,82,207,66,158,101,164,67,16,146,196,66,38,102,164,67,98,144,87,188,66,38,102,164,67,112,205,185,66,126,95,164,67,176,213,182,66,34,64,164,67,98,208,110,159,66,158,72,163,67,240,63,144,66,22,130,160,67,240,100,138,66,
        86,32,156,67,98,16,114,134,66,218,43,153,67,240,47,136,66,94,108,149,67,208,228,142,66,2,114,146,67,98,208,182,143,66,206,20,146,67,240,158,144,66,122,182,145,67,208,232,144,66,102,160,145,67,98,176,62,146,66,74,58,145,67,16,121,149,66,42,18,145,67,80,
        128,159,66,10,235,144,67,98,144,206,162,66,34,222,144,67,144,114,164,66,162,205,144,67,112,206,165,66,150,171,144,67,98,208,60,168,66,178,110,144,67,240,148,171,66,246,202,143,67,112,76,173,66,202,60,143,67,98,80,165,174,66,58,205,142,67,16,20,177,66,
        166,174,141,67,80,140,177,66,70,72,141,67,98,240,190,177,66,54,29,141,67,208,174,177,66,38,23,141,67,16,35,177,66,74,32,141,67,98,112,200,176,66,58,38,141,67,144,1,163,66,98,171,142,67,208,133,146,66,34,129,144,67,108,0,27,105,66,46,215,147,67,108,32,
        36,105,66,246,55,148,67,98,160,47,105,66,250,180,148,67,0,154,103,66,234,19,149,67,128,189,100,66,90,63,149,67,98,192,199,97,66,78,108,149,67,160,86,95,66,130,106,149,67,64,235,91,66,206,56,149,67,98,32,177,86,66,222,236,148,67,192,224,83,66,202,21,148,
        67,32,254,85,66,218,115,147,67,98,32,202,86,66,206,54,147,67,160,189,87,66,66,26,147,67,160,19,90,66,46,249,146,67,98,32,219,93,66,178,195,146,67,64,55,97,66,78,209,146,67,224,207,100,66,170,36,147,67,108,96,88,103,66,98,95,147,67,108,16,180,144,66,42,
        36,144,67,98,176,171,160,66,50,93,142,67,112,159,173,66,202,226,140,67,80,124,173,66,66,219,140,67,98,112,89,173,66,198,211,140,67,208,68,172,66,106,183,140,67,240,21,171,66,94,156,140,67,98,208,59,168,66,38,91,140,67,208,41,164,66,30,217,139,67,208,
        100,161,66,210,102,139,67,98,208,40,160,66,218,51,139,67,48,7,159,66,38,10,139,67,80,225,158,66,38,10,139,67,98,48,169,158,66,38,10,139,67,80,210,143,66,170,138,140,67,64,112,59,66,134,162,145,67,98,96,247,58,66,166,168,145,67,64,155,58,66,246,210,145,
        67,192,153,58,66,14,5,146,67,98,192,153,58,66,210,117,146,67,128,125,57,66,130,185,146,67,96,178,54,66,194,245,146,67,98,64,227,47,66,150,136,147,67,160,109,37,66,238,203,146,67,160,109,37,66,66,190,145,67,98,160,109,37,66,74,26,145,67,224,200,40,66,
        190,191,144,67,64,221,46,66,190,191,144,67,98,32,161,49,66,190,191,144,67,64,17,51,66,26,205,144,67,64,103,53,66,138,252,144,67,108,32,102,56,66,86,57,145,67,108,96,83,59,66,30,21,145,67,98,224,44,66,66,98,192,144,67,144,179,156,66,134,175,138,67,144,
        214,156,66,134,167,138,67,98,176,235,156,66,162,162,138,67,240,208,155,66,42,85,138,67,16,98,154,66,78,251,137,67,98,144,43,152,66,122,112,137,67,208,150,148,66,182,88,136,67,240,165,147,66,26,237,135,67,98,176,105,147,66,42,210,135,67,112,189,146,66,
        238,212,135,67,112,153,142,66,230,1,136,67,108,112,210,137,66,206,53,136,67,108,192,45,132,66,238,192,134,67,108,224,17,125,66,18,76,133,67,108,128,149,116,66,58,4,137,67,98,192,234,111,66,238,15,139,67,128,5,108,66,46,191,140,67,192,237,107,66,154,194,
        140,67,98,160,213,107,66,6,198,140,67,0,108,98,66,126,102,139,67,96,2,87,66,110,181,137,67,108,64,66,66,66,22,162,134,67,108,224,124,60,66,26,75,138,67,98,64,80,57,66,118,78,140,67,32,157,54,66,102,247,141,67,0,125,54,66,106,251,141,67,98,128,92,54,66,
        90,255,141,67,192,240,45,66,74,146,140,67,128,197,35,66,2,208,138,67,98,160,142,17,66,134,169,135,67,128,70,17,66,110,158,135,67,160,214,16,66,86,230,135,67,98,96,152,16,66,126,14,136,67,224,127,14,66,98,208,137,67,0,47,12,66,26,206,139,67,98,32,222,
        9,66,210,203,141,67,96,198,7,66,118,106,143,67,192,136,7,66,138,103,143,67,98,32,75,7,66,162,100,143,67,64,3,252,65,162,246,141,67,64,203,229,65,58,58,140,67,98,0,147,207,65,214,125,138,67,0,211,188,65,150,12,137,67,128,32,188,65,178,5,137,67,98,0,110,
        187,65,214,254,136,67,192,72,186,65,198,28,137,67,0,148,185,65,78,72,137,67,98,192,222,172,65,102,88,140,67,128,239,156,65,114,34,144,67,64,174,156,65,182,39,144,67,98,128,127,156,65,118,43,144,67,192,195,142,65,126,157,143,67,0,85,124,65,46,236,142,
        67,108,0,248,63,65,202,169,141,67,108,0,230,193,64,54,56,142,67,98,0,194,50,64,138,134,142,67,0,0,188,61,246,194,142,67,0,0,105,61,118,190,142,67,98,0,0,0,0,50,183,142,67,0,136,65,64,174,45,142,67,128,42,32,65,230,249,140,67,108,128,116,69,65,62,147,
        140,67,108,0,211,117,65,242,140,141,67,98,192,54,136,65,74,22,142,67,192,66,147,65,166,131,142,67,0,118,147,65,238,127,142,67,98,128,169,147,65,74,124,142,67,192,203,154,65,206,194,140,67,192,80,163,65,250,170,138,67,98,0,214,171,65,38,147,136,67,0,2,
        179,65,138,217,134,67,0,64,179,65,166,213,134,67,98,128,185,179,65,14,206,134,67,192,226,188,65,70,128,135,67,192,57,232,65,118,220,138,67,98,128,132,247,65,246,11,140,67,32,46,2,66,198,4,141,67,64,98,2,66,98,5,141,67,98,192,150,2,66,110,6,141,67,224,
        22,5,66,2,111,139,67,160,241,7,66,246,124,137,67,98,128,204,10,66,234,138,135,67,160,85,13,66,122,225,133,67,32,148,13,66,138,203,133,67,98,32,237,13,66,98,172,133,67,96,32,18,66,206,84,134,67,128,209,32,66,214,206,136,67,108,32,157,51,66,250,249,139,
        67,108,32,154,56,66,242,71,136,67,98,128,88,59,66,162,63,134,67,64,189,61,66,238,149,132,67,192,235,61,66,246,149,132,67,98,64,26,62,66,250,149,132,67,0,158,71,66,170,246,133,67,64,16,83,66,178,165,135,67,98,96,130,94,66,190,84,137,67,64,12,104,66,102,
        181,138,67,128,66,104,66,102,181,138,67,98,128,120,104,66,102,181,138,67,64,151,107,66,110,39,137,67,32,49,111,66,6,65,135,67,98,0,203,114,66,158,90,133,67,32,4,118,66,54,172,131,67,128,90,118,66,142,132,131,67,108,96,247,118,66,122,60,131,67,108,224,
        122,131,66,190,59,133,67,108,0,122,139,66,2,59,135,67,108,112,95,142,66,222,24,135,67,98,48,247,143,66,26,6,135,67,176,68,145,66,126,239,134,67,176,68,145,66,166,230,134,67,98,176,68,145,66,210,221,134,67,48,212,144,66,242,153,134,67,112,74,144,66,210,
        79,134,67,98,144,11,142,66,46,26,133,67,80,163,140,66,194,96,131,67,208,162,140,66,42,213,129,67,98,144,162,140,66,142,221,128,67,208,162,140,66,242,219,128,67,224,227,139,66,190,230,128,67,98,48,78,138,66,226,253,128,67,240,71,128,66,198,91,129,67,32,
        203,126,66,198,91,129,67,98,224,53,125,66,198,91,129,67,64,138,123,66,6,41,129,67,192,207,115,66,38,14,128,67,98,64,204,110,66,76,173,126,67,224,162,106,66,100,123,125,67,32,144,106,66,124,116,125,67,98,0,125,106,66,156,109,125,67,96,50,85,66,252,75,
        125,67,32,63,59,66,228,41,125,67,98,192,232,26,66,116,255,124,67,160,200,11,66,36,221,124,67,224,44,11,66,212,188,124,67,98,224,175,10,66,228,162,124,67,192,73,10,66,20,91,124,67,192,73,10,66,60,29,124,67,98,192,73,10,66,100,223,123,67,96,184,16,66,4,
        225,119,67,224,148,24,66,76,61,115,67,98,128,113,32,66,148,153,110,67,128,31,39,66,100,165,106,67,64,109,39,66,236,115,106,67,98,192,229,39,66,76,39,106,67,160,170,36,66,148,21,105,67,96,124,17,66,148,48,99,67,98,0,34,254,65,132,135,93,67,0,15,246,65,
        28,51,92,67,0,99,246,65,196,216,91,67,98,64,56,247,65,196,243,90,67,192,58,248,65,164,244,90,67,96,194,30,66,52,24,92,67,98,192,99,48,66,148,172,92,67,0,224,62,66,92,33,93,67,32,243,62,66,180,27,93,67,98,32,199,63,66,204,220,92,67,128,8,93,66,204,174,
        76,67,128,8,93,66,92,120,76,67,98,128,8,93,66,12,7,76,67,160,231,96,66,116,242,74,67,128,125,98,66,116,242,74,67,98,0,65,99,66,116,242,74,67,128,89,100,66,156,12,75,67,160,236,100,66,140,44,75,67,98,64,16,102,66,220,107,75,67,240,13,130,66,36,206,78,
        67,64,54,130,66,196,184,78,67,98,0,68,130,66,124,177,78,67,96,220,130,66,12,237,77,67,16,137,131,66,12,4,77,67,98,0,75,133,66,220,164,74,67,128,59,134,66,140,151,73,67,128,37,139,66,60,126,68,67,98,112,130,143,66,52,247,63,67,80,186,145,66,4,120,61,67,
        112,84,146,66,28,104,60,67,98,208,128,146,66,188,25,60,67,144,186,147,66,108,156,58,67,176,13,149,66,204,24,57,67,98,176,96,150,66,44,149,55,67,208,185,151,66,220,216,53,67,80,12,152,66,120,61,53,67,98,176,4,153,66,252,105,51,67,48,50,152,66,108,116,
        50,67,16,168,149,66,108,116,50,67,98,240,221,146,66,108,116,50,67,128,162,134,66,92,55,54,67,96,17,122,66,188,6,58,67,98,32,16,109,66,84,155,60,67,160,129,81,66,68,79,66,67,96,59,72,66,244,86,68,67,98,224,50,62,66,52,137,70,67,96,184,56,66,100,141,71,
        67,96,169,51,66,244,39,72,67,98,0,155,43,66,12,30,73,67,224,8,37,66,180,151,72,67,0,44,33,66,236,173,70,67,108,224,18,32,66,156,34,70,67,108,224,246,37,66,68,27,67,67,98,96,52,41,66,220,112,65,67,96,213,46,66,244,180,62,67,96,121,50,66,252,7,61,67,98,
        32,209,69,66,148,32,52,67,224,101,72,66,228,190,50,67,192,117,80,66,216,251,44,67,98,32,137,86,66,104,164,40,67,0,104,88,66,208,152,38,67,224,99,88,66,8,81,36,67,98,224,99,88,66,192,73,34,67,160,17,86,66,208,54,32,67,0,43,83,66,20,159,31,67,98,64,147,
        80,66,112,23,31,67,0,172,76,66,204,196,30,67,64,209,72,66,236,195,30,67,98,192,124,67,66,212,193,30,67,128,15,63,66,92,17,31,67,160,92,52,66,120,47,32,67,108,224,105,43,66,188,30,33,67,108,0,3,38,66,172,180,35,67,98,96,153,26,66,84,43,41,67,128,55,13,
        66,88,63,46,67,96,29,1,66,240,180,49,67,98,192,219,242,65,108,231,51,67,0,142,215,65,164,52,55,67,192,219,206,65,60,235,55,67,98,192,103,183,65,180,215,57,67,64,26,156,65,204,122,58,67,64,107,142,65,36,108,57,67,98,0,31,136,65,148,239,56,67,128,63,132,
        65,156,244,55,67,64,104,131,65,184,162,54,67,98,64,197,130,65,140,162,53,67,192,47,131,65,128,91,53,67,0,84,135,65,92,3,52,67,98,0,121,148,65,120,191,47,67,192,241,165,65,192,125,44,67,64,130,197,65,16,108,40,67,108,64,57,209,65,108,233,38,67,108,128,
        86,194,65,4,209,36,67,98,64,38,186,65,4,170,35,67,128,208,178,65,248,192,34,67,128,9,178,65,32,203,34,67,98,128,66,177,65,76,213,34,67,0,131,161,65,200,168,35,67,192,11,143,65,8,161,36,67,98,0,40,121,65,80,153,37,67,0,136,90,65,232,93,38,67,128,8,90,
        65,248,85,38,67,98,128,137,89,65,16,78,38,67,128,193,105,65,236,101,36,67,128,19,126,65,84,25,34,67,98,192,50,137,65,188,204,31,67,192,130,145,65,48,225,29,67,192,130,145,65,8,213,29,67,98,192,130,145,65,196,200,29,67,64,108,134,65,72,18,28,67,0,189,
        113,65,100,6,26,67,98,128,161,86,65,128,250,23,67,0,227,64,65,48,72,22,67,128,106,65,65,64,65,22,67,98,0,243,65,65,104,58,22,67,128,234,98,65,208,165,22,67,192,87,133,65,28,48,23,67,98,0,58,153,65,108,186,23,67,0,53,170,65,180,34,24,67,192,18,171,65,
        208,23,24,67,98,192,240,171,65,0,13,24,67,64,53,181,65,64,56,22,67,128,171,191,65,72,6,20,67,98,64,33,202,65,80,212,17,67,0,27,211,65,140,8,16,67,64,157,211,65,140,8,16,67,98,192,30,212,65,140,8,16,67,0,161,213,65,44,86,17,67,192,245,214,65,236,237,18,
        67,98,128,74,216,65,176,133,20,67,192,123,217,65,76,211,21,67,64,157,217,65,76,211,21,67,98,128,190,217,65,76,211,21,67,64,21,221,65,112,133,21,67,192,9,225,65,64,38,21,67,98,192,199,240,65,72,171,19,67,32,237,6,66,28,61,18,67,224,35,18,66,88,215,17,
        67,98,96,94,38,66,176,31,17,67,0,34,50,66,224,206,19,67,192,198,51,66,48,132,25,67,108,32,50,52,66,28,249,26,67,108,160,230,55,66,28,215,26,67,98,32,231,61,66,0,160,26,67,160,97,74,66,180,184,26,67,32,34,79,66,16,5,27,67,98,192,215,85,66,228,112,27,67,
        64,229,92,66,240,97,28,67,96,198,96,66,224,95,29,67,98,96,86,103,66,120,13,31,67,128,103,105,66,236,228,32,67,96,189,104,66,4,120,36,67,98,0,155,103,66,16,144,42,67,224,181,97,66,56,2,47,67,0,243,74,66,188,239,58,67,98,160,143,72,66,28,48,60,67,32,198,
        70,66,68,54,61,67,96,250,70,66,68,54,61,67,98,192,189,71,66,68,54,61,67,160,169,82,66,212,58,59,67,160,167,99,66,236,1,56,67,98,64,130,121,66,252,220,51,67,160,84,125,66,196,49,51,67,112,238,132,66,108,82,49,67,98,208,83,139,66,16,105,47,67,48,230,141,
        66,248,195,46,67,112,178,145,66,220,30,46,67,98,48,27,152,66,40,8,45,67,48,33,157,66,168,158,45,67,80,222,159,66,92,199,47,67,98,144,203,160,66,108,130,48,67,112,211,160,66,52,154,48,67,48,210,160,66,160,156,50,67,98,16,206,160,66,56,114,54,67,16,95,
        159,66,156,205,56,67,208,159,152,66,132,6,64,67,98,208,203,150,66,124,251,65,67,176,121,148,66,228,153,68,67,112,119,147,66,84,216,69,67,98,16,117,146,66,196,22,71,67,16,134,144,66,180,82,73,67,80,43,143,66,76,207,74,67,98,16,0,141,66,132,48,77,67,160,
        166,138,66,44,47,80,67,160,166,138,66,164,145,80,67,98,160,166,138,66,116,207,80,67,16,217,142,66,60,155,81,67,144,138,143,66,12,127,81,67,98,112,134,145,66,116,46,81,67,112,209,156,66,124,39,77,67,176,200,166,66,12,53,73,67,98,176,224,168,66,204,96,
        72,67,48,181,171,66,84,73,71,67,144,18,173,66,244,199,70,67,98,48,112,174,66,156,70,70,67,176,87,177,66,76,25,69,67,208,134,179,66,92,42,68,67,98,144,65,184,66,28,37,66,67,80,99,187,66,20,4,65,67,112,38,190,66,4,86,64,67,108,48,47,192,66,220,213,63,67,
        108,80,221,172,66,44,134,49,67,98,16,61,162,66,40,167,41,67,112,139,153,66,132,18,35,67,112,139,153,66,132,230,34,67,98,112,139,153,66,132,186,34,67,112,81,154,66,180,20,34,67,80,67,155,66,12,118,33,67,98,208,52,157,66,168,47,32,67,80,180,165,66,68,167,
        26,67,80,54,166,66,248,84,26,67,98,240,115,166,66,4,46,26,67,48,96,165,66,128,39,25,67,80,77,162,66,196,155,22,67,98,240,247,159,66,20,173,20,67,176,163,157,66,164,200,18,67,80,32,157,66,56,103,18,67,98,16,254,155,66,44,144,17,67,16,238,155,66,228,248,
        16,67,240,239,156,66,244,157,16,67,98,112,153,157,66,52,98,16,67,144,3,159,66,248,93,16,67,80,161,169,66,196,119,16,67,108,240,147,181,66,204,148,16,67,108,80,151,189,66,116,172,11,67,98,240,129,196,66,8,112,7,67,240,185,197,66,48,196,6,67,176,126,198,
        66,192,196,6,67,98,16,252,198,66,192,196,6,67,48,143,199,66,136,211,6,67,176,197,199,66,220,228,6,67,98,16,252,199,66,32,246,6,67,144,66,202,66,152,72,9,67,16,212,204,66,204,13,12,67,98,176,101,207,66,252,210,14,67,48,136,209,66,140,26,17,67,144,146,
        209,66,152,30,17,67,98,208,156,209,66,140,34,17,67,80,37,213,66,240,60,17,67,112,108,217,66,16,89,17,67,98,144,155,222,66,32,123,17,67,48,108,225,66,116,158,17,67,48,221,225,66,252,194,17,67,98,112,79,226,66,248,231,17,67,240,10,233,66,152,194,25,67,
        80,139,246,66,28,227,41,67,108,248,71,5,67,116,204,65,67,108,40,219,6,67,28,0,66,67,98,184,7,9,67,100,71,66,67,200,47,11,67,228,193,66,67,136,71,13,67,244,108,67,67,98,248,64,14,67,148,188,67,67,136,24,15,67,12,254,67,67,152,38,15,67,124,254,67,67,98,
        184,52,15,67,124,254,67,67,24,26,15,67,60,64,67,67,120,235,14,67,180,86,66,67,98,232,188,14,67,52,109,65,67,88,158,14,67,12,168,64,67,136,167,14,67,156,160,64,67,98,200,202,14,67,20,132,64,67,200,245,24,67,20,139,62,67,248,7,25,67,60,157,62,67,98,136,
        18,25,67,212,167,62,67,200,81,25,67,44,191,63,67,184,148,25,67,36,10,65,67,98,152,215,25,67,28,85,66,67,152,22,26,67,52,108,67,67,184,32,26,67,76,118,67,67,98,232,42,26,67,116,128,67,67,120,171,26,67,164,106,67,67,168,62,27,67,212,69,67,67,98,216,147,
        28,67,140,240,66,67,104,226,34,67,12,16,66,67,168,74,37,67,44,224,65,67,98,216,77,38,67,252,203,65,67,168,41,39,67,196,179,65,67,24,51,39,67,68,170,65,67,98,120,60,39,67,180,160,65,67,56,3,39,67,108,86,64,67,184,179,38,67,52,204,62,67,98,56,100,38,67,
        4,66,61,67,232,39,38,67,140,252,59,67,168,45,38,67,244,248,59,67,98,56,74,38,67,76,231,59,67,56,132,49,67,220,171,57,67,168,202,49,67,212,169,57,67,98,200,19,50,67,188,167,57,67,72,53,50,67,12,33,58,67,40,11,51,67,148,50,62,67,108,248,249,51,67,180,189,
        66,67,108,8,147,53,67,76,51,67,67,98,248,115,54,67,4,116,67,67,120,211,55,67,188,237,67,67,8,160,56,67,220,65,68,67,98,72,37,59,67,36,75,69,67,136,245,58,67,108,71,69,67,184,231,61,67,20,171,68,67,98,168,89,63,67,100,94,68,67,104,141,64,67,44,27,68,67,
        168,147,64,67,180,21,68,67,98,232,153,64,67,76,16,68,67,72,14,64,67,212,69,65,67,88,93,63,67,28,226,61,67,98,104,172,62,67,92,126,58,67,24,35,62,67,252,176,55,67,56,44,62,67,220,167,55,67,98,88,53,62,67,188,158,55,67,168,12,65,67,112,4,55,67,104,124,
        68,67,240,80,54,67,98,168,206,72,67,64,111,53,67,232,196,74,67,0,25,53,67,168,216,74,67,60,57,53,67,98,120,232,74,67,236,82,53,67,72,140,75,67,12,81,56,67,200,68,76,67,196,223,59,67,98,200,41,77,67,228,73,64,67,104,165,77,67,156,87,66,67,232,201,77,67,
        156,87,66,67,98,136,7,78,67,156,87,66,67,168,152,88,67,124,57,64,67,24,226,88,67,108,30,64,67,98,216,3,89,67,236,17,64,67,152,178,88,67,172,52,62,67,88,217,87,67,188,9,58,67,98,8,45,87,67,112,187,54,67,216,164,86,67,76,235,51,67,200,170,86,67,112,201,
        51,67,98,216,178,86,67,200,155,51,67,56,88,88,67,36,56,51,67,200,9,93,67,224,71,50,67,98,232,132,96,67,172,149,49,67,24,94,99,67,144,15,49,67,24,94,99,67,216,29,49,67,98,24,94,99,67,236,89,49,67,232,7,102,67,156,70,62,67,136,23,102,67,52,86,62,67,98,
        88,39,102,67,4,102,62,67,72,156,113,67,4,26,60,67,168,177,113,67,220,2,60,67,98,216,182,113,67,60,253,59,67,184,45,113,67,28,66,57,67,200,128,112,67,84,241,53,67,98,216,211,111,67,144,160,50,67,24,85,111,67,232,220,47,67,40,103,111,67,196,204,47,67,98,
        120,134,111,67,192,176,47,67,152,20,123,67,84,81,45,67,136,119,123,67,152,82,45,67,98,200,145,123,67,152,82,45,67,152,200,123,67,200,254,45,67,88,241,123,67,124,208,46,67,98,8,26,124,67,44,162,47,67,104,68,124,67,192,77,48,67,88,79,124,67,192,77,48,67,
        98,88,90,124,67,192,77,48,67,200,254,124,67,244,230,47,67,200,188,125,67,76,105,47,67,98,140,116,129,67,140,253,43,67,76,106,132,67,180,158,42,67,4,123,135,67,112,108,43,67,98,124,43,136,67,184,154,43,67,68,136,136,67,220,160,43,67,180,201,136,67,168,
        130,43,67,98,100,85,137,67,52,66,43,67,188,105,137,67,208,74,43,67,236,119,137,67,28,204,43,67,98,220,130,137,67,212,47,44,67,52,154,137,67,108,80,44,67,44,19,138,67,88,165,44,67,98,228,81,139,67,0,133,45,67,252,213,140,67,156,77,47,67,172,32,142,67,
        248,105,49,67,108,220,115,142,67,240,241,49,67,108,236,107,148,67,156,147,27,67,98,100,78,154,67,68,134,5,67,116,99,154,67,8,52,5,67,180,60,154,67,180,222,4,67,98,100,219,153,67,88,8,4,67,132,201,153,67,228,232,2,67,68,15,154,67,36,4,2,67,98,172,70,154,
        67,80,78,1,67,36,160,154,67,72,249,0,67,20,40,155,67,72,249,0,67,98,172,237,155,67,72,249,0,67,76,114,156,67,168,204,1,67,52,133,156,67,200,37,3,67,98,44,152,156,67,160,127,4,67,188,2,156,67,68,149,5,67,100,51,155,67,124,153,5,67,98,212,246,154,67,144,
        155,5,67,196,206,154,67,188,174,5,67,196,206,154,67,204,203,5,67,98,196,206,154,67,88,230,5,67,172,167,154,67,160,131,6,67,228,119,154,67,80,41,7,67,98,28,72,154,67,0,207,7,67,116,150,151,67,204,222,17,67,84,123,148,67,80,133,29,67,108,156,213,142,67,
        12,180,50,67,108,204,93,143,67,204,241,51,67,98,188,168,143,67,140,160,52,67,132,18,144,67,0,181,53,67,244,72,144,67,16,88,54,67,98,220,185,144,67,116,170,55,67,92,138,145,67,36,200,58,67,92,138,145,67,180,37,59,67,98,92,138,145,67,20,72,59,67,140,145,
        145,67,228,99,59,67,60,154,145,67,132,99,59,67,98,252,171,145,67,132,99,59,67,180,162,148,67,28,80,48,67,252,150,148,67,124,58,48,67,98,236,146,148,67,0,51,48,67,236,34,148,67,196,90,48,67,52,158,147,67,4,147,48,67,98,132,25,147,67,68,203,48,67,108,170,
        146,67,4,244,48,67,116,167,146,67,160,237,48,67,98,76,158,146,67,20,218,48,67,84,9,146,67,160,241,42,67,100,9,146,67,208,168,42,67,98,100,9,146,67,212,114,42,67,116,78,146,67,88,73,42,67,28,87,147,67,56,224,41,67,98,148,14,148,67,88,151,41,67,220,187,
        148,67,160,86,41,67,36,216,148,67,112,80,41,67,98,116,9,149,67,160,69,41,67,140,13,149,67,248,88,41,67,148,60,149,67,148,40,43,67,98,148,87,149,67,108,50,44,67,20,112,149,67,180,16,45,67,28,115,149,67,124,22,45,67,98,228,119,149,67,160,31,45,67,148,40,
        150,67,48,142,42,67,172,154,152,67,100,90,33,67,108,44,44,153,67,4,55,31,67,108,12,203,152,67,32,84,31,67,98,108,113,152,67,4,111,31,67,20,105,152,67,192,105,31,67,212,94,152,67,140,15,31,67,98,204,53,152,67,16,167,29,67,236,44,152,67,68,45,29,67,244,
        57,152,67,76,21,29,67,98,28,72,152,67,68,251,28,67,156,19,153,67,64,156,28,67,44,61,153,67,64,156,28,67,98,28,72,153,67,64,156,28,67,44,87,153,67,136,232,28,67,156,94,153,67,180,69,29,67,98,20,102,153,67,232,162,29,67,20,114,153,67,0,239,29,67,84,121,
        153,67,196,238,29,67,98,156,128,153,67,196,238,29,67,244,91,154,67,40,205,26,67,220,96,155,67,236,249,22,67,98,44,248,156,67,248,0,17,67,180,54,157,67,60,254,15,67,124,27,157,67,4,210,15,67,98,140,177,156,67,8,38,15,67,236,150,156,67,100,222,13,67,140,
        223,156,67,48,240,12,67,98,28,84,157,67,200,113,11,67,20,166,158,67,124,130,11,67,60,44,159,67,64,13,13,67,98,68,177,159,67,200,148,14,67,228,10,159,67,244,127,16,67,92,254,157,67,84,136,16,67,108,164,169,157,67,4,139,16,67,108,76,166,153,67,220,157,
        31,67,98,76,81,150,67,216,33,44,67,20,164,149,67,96,200,46,67,180,169,149,67,84,60,47,67,98,108,175,149,67,88,178,47,67,172,168,149,67,32,202,47,67,28,128,149,67,232,213,47,67,98,116,89,149,67,236,224,47,67,140,42,149,67,248,111,48,67,100,150,148,67,
        76,158,50,67,98,116,48,148,67,120,30,52,67,36,206,147,67,156,110,53,67,236,187,147,67,60,137,53,67,98,180,169,147,67,228,163,53,67,100,108,147,67,236,136,54,67,180,51,147,67,44,134,55,67,98,252,250,146,67,108,131,56,67,20,150,146,67,172,32,58,67,108,
        83,146,67,116,28,59,67,108,68,218,145,67,52,230,60,67,108,108,253,145,67,196,224,61,67,98,196,16,146,67,148,106,62,67,228,45,146,67,4,128,63,67,44,62,146,67,76,73,64,67,98,124,78,146,67,148,18,65,67,188,94,146,67,196,187,65,67,92,98,146,67,68,193,65,
        67,98,252,101,146,67,172,198,65,67,204,32,147,67,52,226,65,67,164,1,148,67,76,254,65,67,98,116,226,148,67,92,26,66,67,68,168,149,67,252,59,66,67,44,185,149,67,252,72,66,67,98,68,210,149,67,84,92,66,67,244,215,149,67,108,42,66,67,244,215,149,67,92,59,
        65,67,108,244,215,149,67,36,22,64,67,108,28,140,149,67,204,205,63,67,98,132,25,149,67,132,96,63,67,12,90,147,67,60,6,61,67,108,78,147,67,164,201,60,67,98,68,67,147,67,148,143,60,67,100,188,147,67,116,163,57,67,68,6,148,67,236,89,56,67,108,140,52,148,
        67,140,139,55,67,108,92,246,148,67,188,168,55,67,98,244,96,149,67,188,184,55,67,36,246,149,67,68,215,55,67,220,65,150,67,148,236,55,67,98,116,75,151,67,44,55,56,67,180,83,151,67,148,50,56,67,180,198,151,67,4,22,55,67,98,68,255,151,67,4,138,54,67,36,111,
        152,67,24,139,53,67,60,191,152,67,132,223,52,67,108,228,80,153,67,148,167,51,67,108,148,27,153,67,44,111,50,67,98,68,228,152,67,0,43,49,67,68,161,152,67,56,47,47,67,164,137,152,67,8,29,46,67,98,52,124,152,67,88,129,45,67,244,125,152,67,68,124,45,67,180,
        246,152,67,192,225,44,67,98,44,58,153,67,96,139,44,67,236,192,153,67,20,240,43,67,44,34,154,67,152,136,43,67,108,244,210,154,67,120,204,42,67,108,28,117,155,67,36,15,44,67,98,76,206,155,67,160,192,44,67,236,70,156,67,188,196,45,67,44,129,156,67,44,81,
        46,67,98,28,250,156,67,196,116,47,67,212,209,156,67,172,105,47,67,92,104,158,67,96,215,46,67,98,132,232,158,67,68,169,46,67,68,124,159,67,164,119,46,67,180,176,159,67,40,105,46,67,108,4,16,160,67,192,78,46,67,108,196,56,160,67,68,71,45,67,98,44,79,160,
        67,88,182,44,67,92,149,160,67,216,114,43,67,196,212,160,67,92,120,42,67,98,76,92,161,67,184,96,40,67,188,52,161,67,244,122,40,67,52,221,162,67,4,31,41,67,108,4,250,163,67,32,141,41,67,108,172,8,164,67,112,80,42,67,98,172,16,164,67,224,187,42,67,148,30,
        164,67,152,18,44,67,108,39,164,67,4,74,45,67,98,132,53,164,67,176,56,47,67,44,61,164,67,152,139,47,67,60,100,164,67,176,217,47,67,98,212,124,164,67,224,10,48,67,28,20,165,67,44,207,48,67,108,180,165,67,240,141,49,67,108,228,215,166,67,188,232,50,67,108,
        164,214,167,67,156,223,49,67,98,196,98,168,67,212,77,49,67,36,3,169,67,200,180,48,67,20,59,169,67,140,139,48,67,108,180,160,169,67,156,64,48,67,108,100,246,169,67,8,230,48,67,98,20,117,170,67,184,218,49,67,116,91,171,67,244,35,52,67,116,91,171,67,40,
        113,52,67,98,116,91,171,67,64,174,52,67,252,168,170,67,88,186,54,67,60,232,169,67,52,179,56,67,98,20,183,169,67,244,51,57,67,60,159,169,67,116,154,57,67,180,165,169,67,44,209,57,67,98,52,171,169,67,12,0,58,67,12,230,169,67,100,80,59,67,108,40,170,67,
        148,188,60,67,108,20,161,170,67,172,82,63,67,108,12,66,171,67,156,134,63,67,98,132,33,172,67,180,206,63,67,180,131,173,67,212,154,64,67,172,153,173,67,44,224,64,67,98,196,163,173,67,220,255,64,67,12,172,173,67,164,71,65,67,68,172,173,67,172,127,65,67,
        108,68,172,173,67,148,229,65,67,108,28,218,174,67,244,201,65,67,98,36,128,175,67,204,186,65,67,44,65,176,67,68,174,65,67,20,135,176,67,44,174,65,67,108,44,6,177,67,252,173,65,67,108,204,125,176,67,244,44,65,67,98,228,227,174,67,36,169,63,67,12,199,173,
        67,52,29,61,67,100,52,173,67,84,167,57,67,98,124,4,173,67,220,133,56,67,252,251,172,67,44,7,56,67,68,250,172,67,216,66,54,67,98,196,247,172,67,112,191,51,67,180,15,173,67,4,234,50,67,108,155,173,67,212,164,48,67,98,212,247,173,67,120,36,47,67,244,29,
        174,67,16,186,46,67,244,189,174,67,16,122,45,67,98,244,93,175,67,8,58,44,67,36,147,175,67,200,237,43,67,84,83,176,67,4,53,43,67,98,236,117,177,67,156,29,42,67,164,224,177,67,200,237,41,67,84,34,179,67,184,242,41,67,98,124,4,180,67,64,246,41,67,212,67,
        180,67,28,7,42,67,148,212,180,67,4,103,42,67,98,172,166,182,67,160,155,43,67,180,8,184,67,176,40,46,67,220,184,184,67,216,148,49,67,98,76,10,185,67,208,41,51,67,12,52,185,67,48,37,53,67,220,37,185,67,120,192,54,67,98,124,253,184,67,204,81,59,67,92,187,
        183,67,156,18,63,67,204,174,181,67,44,22,65,67,108,140,246,180,67,68,203,65,67,108,164,130,182,67,156,229,65,67,108,180,14,184,67,252,255,65,67,108,76,117,184,67,36,56,65,67,98,140,144,187,67,236,42,59,67,4,154,187,67,88,47,49,67,60,138,184,67,184,33,
        43,67,98,228,203,183,67,88,169,41,67,156,78,183,67,8,248,40,67,252,99,182,67,200,22,40,67,98,52,41,181,67,136,232,38,67,124,141,180,67,60,165,38,67,52,13,179,67,60,165,38,67,98,220,144,177,67,60,165,38,67,252,234,176,67,28,235,38,67,108,191,175,67,140,
        9,40,67,98,84,206,173,67,208,228,41,67,228,126,172,67,92,213,44,67,148,162,171,67,124,67,49,67,98,164,144,171,67,200,159,49,67,132,135,171,67,28,147,49,67,132,247,170,67,164,85,48,67,108,100,95,170,67,80,6,47,67,108,180,155,170,67,48,7,46,67,98,212,7,
        171,67,252,61,44,67,212,158,171,67,12,160,42,67,236,119,172,67,224,237,40,67,98,188,160,173,67,48,156,38,67,204,160,174,67,92,88,37,67,156,13,176,67,32,101,36,67,98,100,106,178,67,232,209,34,67,204,223,180,67,192,33,35,67,116,33,183,67,0,75,37,67,98,
        180,91,185,67,40,109,39,67,12,32,187,67,244,123,43,67,4,239,187,67,140,82,48,67,98,228,134,188,67,148,223,51,67,116,130,188,67,140,121,56,67,148,227,187,67,212,32,60,67,98,100,167,187,67,92,131,61,67,140,10,187,67,12,221,63,67,68,156,186,67,164,8,65,
        67,98,148,105,186,67,92,146,65,67,28,64,186,67,132,12,66,67,28,64,186,67,20,24,66,67,98,28,64,186,67,188,35,66,67,12,143,186,67,36,45,66,67,132,239,186,67,36,45,66,67,108,236,158,187,67,36,45,66,67,108,52,12,188,67,116,186,64,67,98,164,11,189,67,252,
        87,61,67,52,125,189,67,116,1,58,67,92,124,189,67,16,232,53,67,98,60,123,189,67,120,57,48,67,76,96,188,67,188,20,43,67,36,76,186,67,56,17,39,67,98,204,195,184,67,168,27,36,67,108,238,182,67,12,55,34,67,172,226,180,67,184,122,33,67,98,164,240,179,67,184,
        35,33,67,196,38,178,67,212,36,33,67,148,48,177,67,0,125,33,67,98,148,233,173,67,164,169,34,67,244,3,171,67,164,31,39,67,76,145,169,67,196,57,45,67,98,76,74,169,67,28,101,46,67,196,56,169,67,156,143,46,67,188,235,168,67,32,203,46,67,98,84,188,168,67,184,
        239,46,67,28,47,168,67,184,113,47,67,228,177,167,67,252,235,47,67,98,84,189,166,67,192,218,48,67,20,183,166,67,0,217,48,67,84,225,166,67,100,178,47,67,108,116,244,166,67,228,44,47,67,108,124,156,166,67,204,124,47,67,98,20,108,166,67,192,168,47,67,172,
        55,166,67,32,205,47,67,4,40,166,67,160,205,47,67,98,124,253,165,67,180,207,47,67,84,37,165,67,100,194,46,67,204,44,165,67,112,149,46,67,98,84,48,165,67,4,128,46,67,236,88,165,67,16,88,46,67,236,134,165,67,156,60,46,67,98,60,131,166,67,12,166,45,67,28,
        110,167,67,152,77,44,67,12,197,167,67,156,242,42,67,98,12,227,167,67,228,122,42,67,60,42,168,67,144,125,41,67,60,99,168,67,164,191,40,67,98,76,246,168,67,140,213,38,67,172,18,169,67,92,112,37,67,12,213,168,67,156,37,35,67,98,220,163,168,67,12,81,33,67,
        12,50,168,67,8,177,31,67,28,132,167,67,64,86,30,67,98,4,207,166,67,68,237,28,67,140,53,166,67,224,55,28,67,76,90,165,67,184,199,27,67,98,76,163,163,67,44,231,26,67,4,2,162,67,204,201,27,67,76,193,160,67,60,70,30,67,98,140,201,159,67,8,50,32,67,44,86,
        159,67,40,89,34,67,44,86,159,67,204,12,37,67,98,44,86,159,67,52,128,38,67,236,117,159,67,44,183,39,67,228,188,159,67,164,250,40,67,108,148,237,159,67,196,216,41,67,108,4,178,159,67,36,223,42,67,108,124,118,159,67,136,229,43,67,108,164,58,159,67,152,41,
        43,67,98,52,92,158,67,180,110,40,67,132,33,158,67,252,165,36,67,156,164,158,67,236,125,33,67,98,212,158,159,67,136,119,27,67,244,217,162,67,176,33,24,67,132,221,165,67,48,9,26,67,98,228,4,168,67,140,101,27,67,124,150,169,67,72,51,31,67,204,217,169,67,
        244,178,35,67,108,132,232,169,67,16,175,36,67,108,148,60,170,67,96,29,36,67,108,164,144,170,67,176,139,35,67,108,140,110,170,67,228,65,34,67,98,236,65,170,67,48,146,32,67,4,206,169,67,244,163,30,67,4,60,169,67,188,38,29,67,98,140,73,168,67,140,173,26,
        67,4,240,166,67,0,1,25,67,12,98,165,67,172,95,24,67,98,212,123,164,67,96,2,24,67,148,28,163,67,140,32,24,67,188,75,162,67,144,163,24,67,98,116,67,160,67,0,234,25,67,100,178,158,67,168,231,28,67,52,7,158,67,112,201,32,67,98,100,100,157,67,120,122,36,67,
        180,156,157,67,196,141,40,67,4,160,158,67,164,216,43,67,98,20,196,158,67,232,77,44,67,28,222,158,67,136,179,44,67,220,217,158,67,116,186,44,67,98,44,204,158,67,184,208,44,67,20,217,157,67,184,40,45,67,92,169,157,67,184,40,45,67,98,68,114,157,67,184,40,
        45,67,44,37,157,67,8,35,44,67,204,203,156,67,172,56,42,67,108,172,144,156,67,64,244,40,67,108,164,87,156,67,72,213,41,67,108,156,30,156,67,80,182,42,67,108,68,235,155,67,212,85,42,67,108,236,183,155,67,92,245,41,67,108,244,11,156,67,208,189,40,67,108,
        244,95,156,67,72,134,39,67,108,172,104,156,67,212,214,36,67,98,196,114,156,67,204,184,33,67,116,148,156,67,208,144,32,67,188,42,157,67,164,46,30,67,98,36,215,157,67,188,114,27,67,244,248,158,67,72,10,25,67,84,80,160,67,68,125,23,67,108,116,206,160,67,
        104,235,22,67,108,252,196,162,67,16,144,15,67,108,132,187,164,67,180,52,8,67,108,148,125,164,67,156,167,7,67,98,4,41,164,67,0,231,6,67,124,24,164,67,68,29,6,67,204,76,164,67,188,83,5,67,98,164,125,164,67,112,151,4,67,204,9,165,67,192,243,3,67,36,122,
        165,67,192,243,3,67,98,212,38,166,67,192,243,3,67,108,227,166,67,100,52,5,67,108,227,166,67,12,90,6,67,98,108,227,166,67,60,126,7,67,92,71,166,67,200,149,8,67,228,151,165,67,220,171,8,67,98,124,64,165,67,224,182,8,67,68,50,165,67,24,200,8,67,252,6,165,
        67,20,91,9,67,98,196,169,164,67,188,151,10,67,228,149,161,67,84,50,22,67,12,156,161,67,220,61,22,67,98,124,159,161,67,76,68,22,67,204,233,161,67,76,36,22,67,44,65,162,67,188,246,21,67,98,148,152,162,67,52,201,21,67,236,72,163,67,44,153,21,67,12,201,163,
        67,8,140,21,67,98,212,65,167,67,208,48,21,67,76,112,170,67,140,159,25,67,28,135,171,67,92,85,32,67,98,148,161,171,67,124,248,32,67,60,185,171,67,148,131,33,67,172,187,171,67,132,138,33,67,98,36,190,171,67,92,145,33,67,12,234,171,67,116,94,33,67,92,29,
        172,67,52,25,33,67,98,244,176,175,67,92,69,28,67,180,172,180,67,188,157,27,67,228,141,184,67,212,118,31,67,98,116,91,186,67,160,64,33,67,148,7,188,67,52,11,36,67,52,42,189,67,240,40,39,67,98,60,112,189,67,44,233,39,67,148,175,189,67,92,134,40,67,252,
        182,189,67,64,134,40,67,98,92,190,189,67,32,134,40,67,244,116,191,67,140,32,41,67,172,133,193,67,96,221,41,67,98,100,149,196,67,84,245,42,67,76,74,197,67,44,42,43,67,28,89,197,67,140,251,42,67,98,20,99,197,67,28,220,42,67,52,149,198,67,96,221,38,67,68,
        1,200,67,212,26,34,67,98,20,232,201,67,136,189,27,67,20,155,202,67,100,135,25,67,188,165,202,67,96,191,25,67,98,188,173,202,67,64,233,25,67,92,243,202,67,164,177,29,67,140,64,203,67,100,39,34,67,98,180,141,203,67,32,157,38,67,52,207,203,67,16,84,42,67,
        20,210,203,67,140,104,42,67,98,252,212,203,67,32,125,42,67,20,97,204,67,172,170,41,67,132,9,205,67,20,149,40,67,98,180,144,207,67,112,106,36,67,148,39,210,67,184,141,32,67,60,144,212,67,36,91,29,67,98,84,173,216,67,60,229,23,67,196,161,221,67,68,151,
        18,67,236,140,224,67,216,133,16,67,108,92,83,225,67,60,249,15,67,108,164,70,225,67,204,34,15,67,98,92,51,225,67,76,221,13,67,76,204,224,67,216,100,12,67,228,62,224,67,120,95,11,67,98,116,83,223,67,76,172,9,67,124,37,222,67,212,232,8,67,60,246,220,67,
        80,63,9,67,98,196,213,218,67,152,218,9,67,124,60,217,67,160,226,11,67,116,125,216,67,228,237,14,67,98,44,247,215,67,132,17,17,67,84,5,216,67,172,154,18,67,236,166,216,67,116,110,19,67,98,20,47,217,67,224,32,20,67,84,41,218,67,200,203,19,67,244,197,218,
        67,200,181,18,67,98,244,21,219,67,216,39,18,67,12,34,219,67,168,251,17,67,20,35,219,67,96,98,17,67,98,28,36,219,67,124,1,17,67,36,39,219,67,28,178,16,67,140,42,219,67,8,178,16,67,98,140,60,219,67,8,178,16,67,84,110,219,67,236,232,17,67,84,110,219,67,
        224,89,18,67,98,84,110,219,67,8,14,19,67,28,43,219,67,4,4,20,67,124,204,218,67,48,170,20,67,98,20,9,218,67,80,1,22,67,148,5,217,67,156,77,22,67,4,57,216,67,8,108,21,67,98,156,100,215,67,208,129,20,67,244,1,215,67,20,129,19,67,12,225,214,67,220,236,17,
        67,98,236,143,214,67,12,8,14,67,68,85,216,67,220,189,8,67,68,186,218,67,212,109,6,67,98,244,219,219,67,12,86,5,67,156,137,220,67,64,9,5,67,52,223,221,67,232,9,5,67,98,212,36,223,67,232,9,5,67,148,156,223,67,144,60,5,67,180,131,224,67,152,36,6,67,98,204,
        129,225,67,172,35,7,67,60,67,226,67,112,144,8,67,164,198,226,67,68,104,10,67,98,76,4,227,67,184,69,11,67,124,99,227,67,172,67,13,67,220,126,227,67,104,67,14,67,98,60,136,227,67,60,155,14,67,36,149,227,67,96,180,14,67,252,175,227,67,136,163,14,67,98,44,
        17,228,67,172,102,14,67,244,228,229,67,4,222,13,67,212,202,230,67,80,187,13,67,98,132,237,231,67,116,143,13,67,180,211,233,67,104,189,13,67,132,224,234,67,144,29,14,67,98,28,24,238,67,244,68,15,67,52,29,241,67,112,154,19,67,28,71,242,67,108,188,24,67,
        98,124,92,243,67,220,131,29,67,164,11,243,67,212,80,36,67,188,119,241,67,36,46,42,67,98,164,218,239,67,144,45,48,67,244,16,237,67,184,158,52,67,244,6,234,67,0,9,54,67,98,236,25,233,67,100,119,54,67,132,3,231,67,32,120,54,67,212,79,230,67,24,11,54,67,
        98,28,180,229,67,240,171,53,67,252,243,228,67,172,237,52,67,220,109,228,67,196,45,52,67,98,124,54,228,67,144,222,51,67,52,5,228,67,188,157,51,67,92,0,228,67,188,157,51,67,98,116,251,227,67,188,157,51,67,116,231,227,67,112,254,51,67,212,211,227,67,156,
        116,52,67,98,52,165,227,67,52,141,53,67,76,246,226,67,84,149,55,67,180,133,226,67,28,86,56,67,98,180,177,225,67,20,193,57,67,244,116,224,67,132,120,58,67,92,120,223,67,148,26,58,67,98,124,186,222,67,236,211,57,67,164,30,222,67,92,42,57,67,124,137,221,
        67,12,0,56,67,98,212,155,220,67,180,36,54,67,116,6,220,67,196,111,51,67,52,243,219,67,60,163,48,67,98,108,233,219,67,44,54,47,67,4,2,220,67,248,211,44,67,100,36,220,67,44,225,43,67,98,204,43,220,67,188,172,43,67,156,41,220,67,216,129,43,67,100,31,220,
        67,216,129,43,67,98,4,1,220,67,216,129,43,67,12,168,218,67,248,42,45,67,92,221,217,67,68,74,46,67,98,188,106,214,67,16,45,51,67,92,171,209,67,60,12,61,67,44,130,206,67,28,231,69,67,98,44,93,205,67,220,27,73,67,228,117,205,67,76,116,72,67,52,160,205,67,
        188,226,75,67,98,60,180,205,67,148,130,77,67,52,197,205,67,60,37,79,67,236,197,205,67,44,133,79,67,108,44,199,205,67,132,51,80,67,108,156,4,205,67,188,166,78,67,98,156,153,204,67,132,204,77,67,76,60,204,67,252,25,77,67,68,53,204,67,252,25,77,67,98,44,
        46,204,67,252,25,77,67,172,9,204,67,28,133,77,67,20,228,203,67,252,7,78,67,108,188,159,203,67,252,245,78,67,108,236,193,203,67,124,78,80,67,98,20,213,203,67,60,15,81,67,52,234,203,67,148,8,83,67,236,241,203,67,140,201,84,67,108,188,255,203,67,12,236,
        87,67,108,204,131,204,67,60,0,87,67,98,12,127,208,67,12,228,79,67,148,47,216,67,140,181,71,67,36,13,221,67,236,103,69,67,98,76,140,221,67,180,43,69,67,52,93,222,67,156,225,68,67,84,221,222,67,60,195,68,67,98,196,165,223,67,188,147,68,67,116,229,223,67,
        172,111,68,67,196,164,224,67,164,193,67,67,98,4,15,229,67,76,189,63,67,4,53,232,67,148,154,62,67,156,242,234,67,28,10,64,67,98,140,38,236,67,108,171,64,67,4,133,237,67,124,41,66,67,44,60,238,67,140,159,67,67,108,236,160,238,67,84,109,68,67,108,228,121,
        239,67,108,51,68,67,98,140,214,243,67,108,9,67,67,132,62,248,67,20,15,72,67,92,186,249,67,196,226,79,67,98,44,249,249,67,36,46,81,67,252,255,249,67,4,126,81,67,180,255,249,67,28,15,83,67,98,180,255,249,67,252,45,85,67,36,217,249,67,212,103,86,67,28,87,
        249,67,92,121,88,67,98,44,130,248,67,132,220,91,67,44,3,247,67,212,192,94,67,84,160,244,67,188,149,97,67,98,204,100,243,67,60,12,99,67,124,146,242,67,116,159,99,67,92,142,241,67,220,187,99,67,98,244,10,240,67,36,230,99,67,116,60,239,67,212,198,98,67,
        220,21,239,67,228,75,96,67,98,140,247,238,67,252,88,94,67,220,178,239,67,172,120,91,67,148,167,240,67,228,32,90,67,98,132,214,240,67,244,222,89,67,124,204,240,67,100,1,90,67,60,129,240,67,132,164,90,67,98,68,127,239,67,244,211,92,67,68,34,239,67,92,78,
        96,67,12,194,239,67,60,202,97,67,98,172,10,240,67,212,118,98,67,220,135,240,67,44,242,98,67,124,238,240,67,44,242,98,67,98,172,223,241,67,44,242,98,67,228,39,244,67,244,249,96,67,196,106,245,67,36,19,95,67,98,44,204,246,67,76,254,92,67,20,74,248,67,108,
        105,89,67,252,199,248,67,164,255,86,67,98,132,87,249,67,76,63,84,67,100,26,249,67,196,193,80,67,84,42,248,67,188,254,77,67,98,132,181,247,67,180,166,76,67,124,166,246,67,228,131,74,67,124,3,246,67,84,167,73,67,98,124,11,245,67,196,87,72,67,228,242,243,
        67,52,102,71,67,132,174,242,67,4,201,70,67,98,60,60,242,67,164,145,70,67,52,112,239,67,156,117,70,67,60,87,239,67,140,167,70,67,98,228,80,239,67,68,180,70,67,188,94,239,67,180,56,71,67,252,117,239,67,12,206,71,67,98,156,169,239,67,212,25,73,67,60,174,
        239,67,28,45,75,67,44,128,239,67,228,94,76,67,98,28,20,239,67,148,43,79,67,124,219,237,67,156,113,80,67,60,89,236,67,116,168,79,67,98,44,23,235,67,180,0,79,67,236,61,234,67,84,38,77,67,236,61,234,67,220,14,75,67,98,236,61,234,67,12,219,73,67,140,94,234,
        67,12,67,73,67,124,220,234,67,220,43,72,67,98,212,81,235,67,204,39,71,67,156,195,235,67,20,131,70,67,228,136,236,67,220,191,69,67,108,236,21,237,67,76,52,69,67,108,252,183,236,67,164,169,68,67,98,140,111,235,67,228,196,66,67,36,44,233,67,180,202,65,67,
        4,86,231,67,164,86,66,67,98,20,4,230,67,60,187,66,67,28,223,227,67,244,223,67,67,180,171,226,67,116,211,68,67,108,36,108,226,67,204,5,69,67,108,236,238,226,67,84,101,69,67,98,92,204,228,67,52,194,70,67,28,15,230,67,20,121,74,67,172,42,230,67,252,235,
        78,67,98,244,63,230,67,164,90,82,67,12,182,229,67,44,189,84,67,4,55,228,67,220,143,87,67,98,100,22,227,67,92,176,89,67,196,193,225,67,44,73,91,67,108,69,224,67,124,75,92,67,98,92,18,223,67,12,28,93,67,172,82,222,67,188,92,93,67,252,248,220,67,140,104,
        93,67,108,36,190,219,67,92,115,93,67,108,12,66,219,67,4,42,95,67,108,244,197,218,67,172,224,96,67,108,36,24,219,67,148,253,96,67,98,76,37,220,67,52,92,97,67,52,199,220,67,172,128,98,67,236,29,221,67,244,164,100,67,98,52,197,221,67,92,198,104,67,116,239,
        221,67,244,13,107,67,172,240,221,67,20,5,112,67,98,180,241,221,67,164,40,114,67,108,234,221,67,84,168,116,67,172,225,221,67,148,146,117,67,108,188,209,221,67,124,60,119,67,108,132,41,224,67,124,111,119,67,98,108,115,225,67,140,139,119,67,244,238,226,
        67,140,191,119,67,244,116,227,67,252,226,119,67,98,236,75,229,67,172,95,120,67,116,57,231,67,68,146,121,67,148,48,232,67,180,211,122,67,98,212,90,233,67,156,87,124,67,220,139,233,67,116,83,127,67,4,168,232,67,198,12,129,67,98,60,104,232,67,46,112,129,
        67,172,114,231,67,154,78,130,67,172,132,230,67,130,252,130,67,98,44,189,229,67,82,142,131,67,228,123,228,67,230,70,132,67,124,107,226,67,74,87,133,67,108,116,110,224,67,182,93,134,67,108,164,174,224,67,94,193,134,67,98,244,209,224,67,46,248,134,67,60,
        16,225,67,38,91,135,67,4,57,225,67,74,157,135,67,98,212,145,225,67,90,45,136,67,148,242,225,67,198,113,136,67,84,129,226,67,114,133,136,67,98,68,41,227,67,150,156,136,67,92,172,227,67,26,223,136,67,244,18,228,67,50,81,137,67,98,172,191,228,67,82,17,138,
        67,236,223,228,67,250,9,139,67,228,102,228,67,134,216,139,67,98,156,22,228,67,122,97,140,67,156,13,228,67,230,13,141,67,140,70,228,67,230,67,142,67,98,28,151,228,67,138,250,143,67,212,126,228,67,110,179,144,67,68,223,227,67,198,86,145,67,98,180,91,227,
        67,106,221,145,67,140,197,226,67,34,19,146,67,204,209,225,67,186,18,146,67,98,212,247,224,67,186,18,146,67,252,107,224,67,82,244,145,67,148,222,222,67,142,112,145,67,98,140,134,221,67,122,254,144,67,188,94,220,67,34,107,144,67,68,7,218,67,66,7,143,67,
        108,28,86,216,67,38,6,142,67,108,148,28,216,67,118,74,142,67,98,244,252,215,67,10,112,142,67,68,168,215,67,86,233,142,67,108,96,215,67,2,88,143,67,98,76,184,213,67,26,229,145,67,44,82,213,67,170,117,146,67,196,135,212,67,46,95,147,67,98,196,124,211,67,
        62,147,148,67,196,179,210,67,134,65,149,67,164,222,209,67,178,173,149,67,98,228,183,208,67,74,67,150,67,204,94,207,67,74,39,150,67,244,161,206,67,118,106,149,67,98,140,54,206,67,6,255,148,67,52,158,205,67,6,206,147,67,236,85,205,67,174,209,146,67,98,
        148,57,205,67,166,110,146,67,236,30,205,67,230,29,146,67,180,26,205,67,54,30,146,67,98,124,252,204,67,130,32,146,67,4,224,203,67,170,159,146,67,252,223,203,67,230,170,146,67,98,252,223,203,67,174,178,146,67,220,29,204,67,170,205,146,67,148,105,204,67,
        222,230,146,67,98,84,181,204,67,18,0,147,67,76,243,204,67,166,23,147,67,76,243,204,67,70,27,147,67,98,76,243,204,67,238,30,147,67,52,0,204,67,34,141,147,67,36,215,202,67,66,16,148,67,98,12,174,201,67,98,147,148,67,244,186,200,67,102,4,149,67,244,186,
        200,67,110,11,149,67,98,244,186,200,67,126,18,149,67,212,163,204,67,198,120,150,67,36,107,209,67,174,39,152,67,98,108,50,214,67,154,214,153,67,76,27,218,67,10,61,155,67,76,27,218,67,58,68,155,67,98,76,27,218,67,98,75,155,67,196,161,217,67,194,112,155,
        67,60,13,217,67,62,151,155,67,98,180,120,216,67,182,189,155,67,4,248,215,67,86,227,155,67,76,239,215,67,214,234,155,67,98,140,230,215,67,82,242,155,67,28,62,216,67,38,25,156,67,228,177,216,67,30,65,156,67,98,172,37,217,67,18,105,156,67,28,130,217,67,
        14,140,156,67,76,127,217,67,218,142,156,67,98,124,124,217,67,170,145,156,67,188,101,215,67,46,35,157,67,252,218,212,67,50,210,157,67,98,68,80,210,67,58,129,158,67,180,56,208,67,138,19,159,67,228,52,208,67,86,23,159,67,98,12,49,208,67,18,27,159,67,148,
        190,211,67,154,219,160,67,236,25,216,67,234,251,162,67,98,92,67,223,67,182,122,166,67,44,185,224,67,78,58,167,67,204,83,231,67,222,201,170,67,98,68,88,235,67,106,244,172,67,4,158,238,67,218,189,174,67,108,153,238,67,110,194,174,67,98,220,148,238,67,2,
        199,174,67,28,239,236,67,178,64,174,67,60,240,234,67,250,151,173,67,98,84,241,232,67,66,239,172,67,92,77,231,67,50,103,172,67,244,74,231,67,158,105,172,67,98,140,72,231,67,2,108,172,67,164,203,232,67,170,63,173,67,52,167,234,67,226,63,174,67,98,196,130,
        236,67,26,64,175,67,28,5,238,67,182,20,176,67,188,1,238,67,86,24,176,67,98,76,254,237,67,250,27,176,67,132,153,234,67,82,1,175,67,12,119,230,67,58,164,173,67,98,148,162,225,67,110,12,172,67,116,59,218,67,86,128,169,67,124,194,209,67,90,132,166,67,98,
        172,129,202,67,86,246,163,67,244,141,196,67,186,227,161,67,108,136,196,67,58,233,161,67,98,236,130,196,67,190,238,161,67,228,145,196,67,74,71,162,67,164,169,196,67,2,174,162,67,98,4,241,196,67,62,226,163,67,116,7,197,67,254,252,164,67,60,252,196,67,38,
        191,166,67,98,140,234,196,67,162,133,169,67,92,110,196,67,250,127,171,67,244,57,195,67,254,234,173,67,98,108,120,193,67,82,113,177,67,92,163,190,67,162,190,179,67,92,30,187,67,254,129,180,67,98,140,210,186,67,110,146,180,67,148,22,186,67,54,157,180,67,
        92,76,185,67,178,156,180,67,98,140,200,183,67,170,155,180,67,244,205,182,67,46,109,180,67,148,83,181,67,206,223,179,67,98,124,165,176,67,34,32,178,67,36,198,171,67,206,139,173,67,52,242,167,67,98,76,167,67,98,100,67,166,67,58,141,164,67,68,148,164,67,
        198,236,160,67,132,136,163,67,146,203,157,67,108,108,31,163,67,2,145,156,67,108,52,167,161,67,46,126,155,67,108,4,47,160,67,86,107,154,67,108,196,223,159,67,226,132,154,67,98,52,180,159,67,238,146,154,67,68,137,159,67,150,162,154,67,92,128,159,67,166,
        167,154,67,98,140,119,159,67,190,172,154,67,4,182,159,67,170,3,155,67,92,11,160,67,210,104,155,67,98,100,43,161,67,74,190,156,67,188,217,161,67,110,196,157,67,100,149,162,67,114,58,159,67,98,52,130,164,67,134,16,163,67,180,205,164,67,130,226,166,67,100,
        123,163,67,190,222,170,67,98,92,245,162,67,242,114,172,67,228,208,161,67,86,131,174,67,252,183,160,67,202,220,175,67,98,172,40,159,67,202,199,177,67,4,139,157,67,6,14,179,67,52,254,154,67,190,96,180,67,98,140,76,154,67,238,188,180,67,44,187,153,67,26,
        15,181,67,44,187,153,67,102,23,181,67,98,44,187,153,67,182,31,181,67,180,244,155,67,70,251,182,67,212,172,158,67,74,56,185,67,98,236,100,161,67,78,117,187,67,204,172,163,67,46,88,189,67,60,190,163,67,86,105,189,67,98,236,233,163,67,54,148,189,67,108,
        39,164,67,242,179,189,67,220,223,156,67,194,221,185,67,108,252,117,150,67,102,124,182,67,108,180,83,149,67,122,219,182,67,98,44,58,146,67,90,223,183,67,204,230,143,67,94,65,184,67,164,210,140,67,242,65,184,67,98,28,47,138,67,242,65,184,67,148,201,136,
        67,146,12,184,67,148,115,134,67,78,76,183,67,98,140,67,133,67,138,234,182,67,68,33,131,67,250,213,181,67,92,111,130,67,190,67,181,67,98,236,36,128,67,170,97,179,67,8,89,125,67,178,165,176,67,72,131,124,67,110,126,173,67,98,8,81,124,67,134,192,172,67,
        104,96,124,67,202,111,170,67,152,154,124,67,106,252,169,67,98,200,180,124,67,146,200,169,67,72,67,124,67,246,168,169,67,232,202,108,67,226,148,165,67,108,72,224,92,67,178,98,161,67,108,184,48,92,67,146,169,161,67,98,40,208,91,67,146,208,161,67,232,247,
        90,67,190,29,162,67,24,80,90,67,10,85,162,67,108,232,30,89,67,158,185,162,67,108,216,117,99,67,122,219,167,67,98,168,37,105,67,30,174,170,67,104,197,109,67,2,1,173,67,120,188,109,67,122,5,173,67,98,136,179,109,67,242,9,173,67,168,51,107,67,182,249,172,
        67,168,46,104,67,102,225,172,67,98,152,41,101,67,18,201,172,67,232,129,98,67,38,184,172,67,56,72,98,67,202,187,172,67,98,8,240,97,67,102,193,172,67,40,68,99,67,14,96,173,67,56,172,106,67,2,166,176,67,98,216,75,112,67,58,34,179,67,168,95,115,67,14,140,
        180,67,216,50,115,67,102,144,180,67,98,56,12,115,67,34,148,180,67,40,243,111,67,50,121,180,67,104,80,108,67,126,84,180,67,98,168,173,104,67,202,47,180,67,136,142,101,67,230,20,180,67,136,96,101,67,186,24,180,67,98,136,27,101,67,114,30,180,67,136,106,
        101,67,66,89,180,67,56,37,103,67,146,105,181,67,98,40,76,104,67,2,31,182,67,136,92,106,67,146,89,183,67,88,187,107,67,150,36,184,67,98,168,202,110,67,242,233,185,67,216,183,113,67,22,189,187,67,88,186,121,67,174,222,192,67,98,120,178,127,67,166,177,196,
        67,252,30,128,67,150,12,197,67,124,16,128,67,162,10,197,67,98,220,12,128,67,162,10,197,67,184,16,116,67,50,239,191,67,24,98,101,67,74,178,185,67,99,109,248,107,111,67,138,247,187,67,98,232,195,109,67,38,239,186,67,136,55,107,67,242,101,185,67,72,194,
        105,67,190,141,184,67,98,152,46,103,67,158,15,183,67,168,128,96,67,138,235,178,67,152,152,96,67,142,223,178,67,98,72,159,96,67,58,220,178,67,136,101,99,67,194,245,178,67,56,195,102,67,42,24,179,67,98,216,32,106,67,154,58,179,67,136,10,109,67,58,84,179,
        67,40,60,109,67,26,81,179,67,98,216,128,109,67,186,76,179,67,120,125,107,67,238,93,178,67,24,211,100,67,34,107,175,67,98,72,1,96,67,90,73,173,67,88,22,92,67,134,135,171,67,88,30,92,67,134,131,171,67,98,72,38,92,67,122,127,171,67,248,246,94,67,230,146,
        171,67,184,95,98,67,162,174,171,67,98,104,200,101,67,90,202,171,67,200,150,104,67,218,222,171,67,24,156,104,67,50,220,171,67,98,56,161,104,67,154,217,171,67,24,189,100,67,94,233,169,67,24,246,95,67,146,141,167,67,108,120,70,87,67,198,67,163,67,108,152,
        149,85,67,42,164,163,67,98,136,167,84,67,50,217,163,67,200,89,83,67,58,30,164,67,8,176,82,67,158,61,164,67,98,72,152,81,67,70,113,164,67,40,129,81,67,222,121,164,67,216,185,81,67,246,152,164,67,98,72,220,81,67,214,171,164,67,56,100,84,67,106,193,165,
        67,168,89,87,67,202,1,167,67,98,40,79,90,67,46,66,168,67,216,187,92,67,174,78,169,67,24,189,92,67,110,86,169,67,98,40,191,92,67,58,94,169,67,24,106,89,67,254,95,169,67,104,87,85,67,98,90,169,67,98,120,53,79,67,242,81,169,67,24,251,77,67,250,84,169,67,
        200,50,78,67,10,108,169,67,98,200,87,78,67,90,123,169,67,88,135,82,67,86,14,171,67,8,128,87,67,134,235,172,67,98,168,120,92,67,186,200,174,67,8,134,96,67,138,80,176,67,56,129,96,67,58,82,176,67,98,120,124,96,67,226,83,176,67,8,21,93,67,210,43,176,67,
        216,240,88,67,34,249,175,67,98,152,204,84,67,114,198,175,67,8,75,81,67,82,159,175,67,40,38,81,67,46,162,175,67,98,88,0,81,67,22,165,175,67,232,45,88,67,190,191,178,67,184,154,97,67,202,191,182,67,98,152,204,106,67,210,166,186,67,216,88,114,67,50,216,
        189,67,184,96,114,67,50,216,189,67,98,168,104,114,67,50,216,189,67,24,20,113,67,230,255,188,67,8,108,111,67,134,247,187,67,99,109,148,249,142,67,214,197,181,67,98,204,89,144,67,6,154,181,67,140,243,146,67,118,20,181,67,44,67,147,67,166,233,180,67,98,
        36,84,147,67,126,224,180,67,116,166,143,67,134,228,178,67,180,215,137,67,202,212,175,67,98,12,154,132,67,142,17,173,67,76,69,128,67,210,204,170,67,212,55,128,67,70,202,170,67,98,236,37,128,67,218,198,170,67,244,31,128,67,90,44,171,67,212,33,128,67,234,
        66,172,67,98,228,35,128,67,110,124,173,67,28,44,128,67,26,221,173,67,52,80,128,67,202,98,174,67,98,156,193,128,67,30,7,176,67,84,132,129,67,214,75,177,67,36,202,130,67,218,131,178,67,98,212,253,132,67,178,159,180,67,4,140,135,67,6,172,181,67,148,32,139,
        67,82,239,181,67,98,12,214,139,67,166,252,181,67,172,250,141,67,142,229,181,67,148,249,142,67,210,197,181,67,99,109,252,130,149,67,22,247,179,67,98,252,130,149,67,62,219,179,67,20,124,143,67,82,13,175,67,20,156,141,67,142,170,173,67,98,220,51,137,67,
        182,104,170,67,28,255,135,67,242,120,169,67,172,130,132,67,150,153,166,67,108,204,113,130,67,202,229,164,67,108,228,255,135,67,126,28,164,67,98,4,14,139,67,198,173,163,67,132,146,141,67,110,78,163,67,28,152,141,67,158,72,163,67,98,188,157,141,67,202,
        66,163,67,4,168,137,67,158,11,161,67,148,203,132,67,62,92,158,67,98,232,57,125,67,26,242,154,67,8,255,119,67,2,118,153,67,152,48,120,67,166,107,153,67,98,152,87,120,67,138,99,153,67,248,30,125,67,14,0,153,67,164,103,129,67,166,142,152,67,98,204,63,132,
        67,66,29,152,67,164,151,134,67,94,189,151,67,156,156,134,67,138,185,151,67,98,188,174,134,67,166,171,151,67,36,82,134,67,174,115,151,67,220,9,134,67,222,96,151,67,98,20,225,133,67,62,86,151,67,228,72,133,67,222,51,151,67,164,183,132,67,122,20,151,67,
        98,20,49,129,67,114,81,150,67,56,187,123,67,202,213,148,67,136,68,118,67,142,206,146,67,98,200,10,116,67,18,251,145,67,72,42,112,67,142,255,143,67,200,158,110,67,126,229,142,67,98,248,239,109,67,226,104,142,67,232,38,109,67,202,218,141,67,216,223,108,
        67,198,169,141,67,98,24,141,108,67,174,112,141,67,8,200,107,67,234,32,141,67,168,187,106,67,242,203,140,67,108,152,24,105,67,70,71,140,67,108,88,196,104,67,102,140,140,67,98,152,155,103,67,246,127,141,67,120,217,101,67,186,192,142,67,216,179,100,67,210,
        113,143,67,98,200,247,99,67,70,227,143,67,232,93,99,67,162,67,144,67,232,93,99,67,246,71,144,67,98,232,93,99,67,82,76,144,67,216,241,103,67,150,132,145,67,72,138,109,67,242,253,146,67,98,168,34,115,67,82,119,148,67,248,182,119,67,214,176,149,67,120,183,
        119,67,170,182,149,67,98,120,183,119,67,126,188,149,67,88,44,114,67,194,89,150,67,184,100,107,67,46,20,151,67,98,40,157,100,67,150,206,151,67,248,16,95,67,62,108,152,67,200,16,95,67,134,114,152,67,98,200,16,95,67,194,120,152,67,40,32,103,67,190,79,154,
        67,120,250,112,67,10,137,156,67,98,200,212,122,67,90,194,158,67,140,121,129,67,114,154,160,67,116,130,129,67,38,162,160,67,98,100,139,129,67,214,169,160,67,200,71,125,67,234,13,161,67,136,28,118,67,138,128,161,67,98,56,241,110,67,38,243,161,67,184,231,
        104,67,202,86,162,67,8,178,104,67,246,93,162,67,98,232,101,104,67,38,104,162,67,88,162,111,67,70,89,164,67,84,208,132,67,90,52,171,67,98,156,249,141,67,126,9,176,67,212,122,149,67,26,254,179,67,188,125,149,67,154,254,179,67,98,172,128,149,67,154,254,
        179,67,12,131,149,67,178,251,179,67,12,131,149,67,30,247,179,67,99,109,28,97,152,67,142,33,179,67,98,44,137,154,67,126,34,178,67,140,225,156,67,86,119,176,67,236,113,158,67,186,208,174,67,98,108,208,161,67,102,66,171,67,196,225,162,67,106,58,167,67,132,
        171,161,67,238,163,162,67,98,204,33,161,67,158,154,160,67,108,28,160,67,206,149,158,67,244,176,158,67,30,192,156,67,98,100,17,158,67,242,241,155,67,52,123,157,67,154,73,155,67,212,110,157,67,18,87,155,67,98,28,99,157,67,222,99,155,67,44,72,154,67,126,
        249,159,67,140,51,154,67,122,28,160,67,98,140,43,154,67,2,42,160,67,172,155,154,67,218,145,161,67,164,44,155,67,30,60,163,67,98,156,189,155,67,102,230,164,67,116,43,156,67,42,67,166,67,188,32,156,67,42,67,166,67,98,124,10,156,67,42,67,166,67,236,34,153,
        67,34,130,165,67,228,78,151,67,218,2,165,67,108,156,21,150,67,170,173,164,67,108,28,168,147,67,34,159,166,67,98,236,64,145,67,138,139,168,67,92,58,145,67,18,144,168,67,140,34,145,67,198,91,168,67,98,12,249,144,67,174,0,168,67,156,225,144,67,254,23,167,
        67,52,225,144,67,134,210,165,67,98,52,225,144,67,198,177,164,67,196,221,144,67,130,153,164,67,180,187,144,67,78,163,164,67,98,76,167,144,67,34,169,164,67,148,19,144,67,130,190,164,67,92,115,143,67,182,210,164,67,98,228,176,142,67,66,235,164,67,36,80,
        142,67,10,0,165,67,36,80,142,67,62,17,165,67,98,36,80,142,67,106,31,165,67,28,111,142,67,110,108,165,67,244,148,142,67,94,188,165,67,98,28,213,142,67,214,67,166,67,204,217,142,67,230,92,166,67,148,217,142,67,42,44,167,67,98,148,217,142,67,70,236,167,
        67,196,209,142,67,178,28,168,67,132,161,142,67,150,143,168,67,98,12,82,142,67,2,77,169,67,76,237,141,67,158,236,169,67,148,103,141,67,190,128,170,67,108,140,243,140,67,74,1,171,67,108,172,48,141,67,150,45,171,67,98,28,100,143,67,194,197,172,67,204,177,
        146,67,54,93,175,67,164,65,150,67,94,74,178,67,98,220,1,151,67,78,232,178,67,236,166,151,67,230,105,179,67,116,176,151,67,90,106,179,67,98,236,185,151,67,90,106,179,67,116,9,152,67,10,74,179,67,36,97,152,67,138,33,179,67,99,109,228,70,187,67,10,217,178,
        67,98,84,220,188,67,118,109,178,67,244,251,189,67,62,192,177,67,20,88,191,67,10,102,176,67,98,4,164,192,67,242,27,175,67,140,111,193,67,186,235,173,67,252,27,194,67,234,67,172,67,98,92,113,195,67,242,252,168,67,132,153,195,67,206,96,165,67,60,143,194,
        67,254,230,161,67,98,180,114,194,67,134,135,161,67,196,78,194,67,250,44,161,67,124,63,194,67,194,29,161,67,98,36,48,194,67,138,14,161,67,92,182,191,67,178,39,160,67,4,191,188,67,202,28,159,67,98,164,199,185,67,226,17,158,67,52,94,183,67,170,51,157,67,
        228,98,183,67,254,46,157,67,98,148,103,183,67,78,42,157,67,180,234,183,67,234,9,157,67,76,134,184,67,6,231,156,67,98,124,244,185,67,226,148,156,67,164,241,185,67,38,150,156,67,4,140,185,67,218,114,156,67,98,100,93,185,67,170,98,156,67,140,240,184,67,
        2,60,156,67,28,154,184,67,238,28,156,67,108,244,252,183,67,110,228,155,67,108,188,118,186,67,142,83,155,67,98,76,211,187,67,226,3,155,67,204,249,188,67,154,191,154,67,44,5,189,67,218,187,154,67,98,156,34,189,67,18,178,154,67,60,116,188,67,118,79,154,
        67,52,173,187,67,54,249,153,67,98,188,92,185,67,114,248,152,67,164,196,182,67,206,228,152,67,116,225,180,67,186,197,153,67,98,84,177,178,67,118,202,154,67,172,24,177,67,250,26,157,67,140,154,176,67,78,249,159,67,98,116,106,176,67,90,17,161,67,12,123,
        176,67,38,102,163,67,60,187,176,67,238,144,164,67,98,156,105,177,67,170,188,167,67,156,38,179,67,54,214,170,67,76,43,181,67,210,122,172,67,98,244,230,182,67,250,227,173,67,76,13,184,67,198,89,174,67,76,167,185,67,42,70,174,67,98,140,67,186,67,174,62,
        174,67,68,115,186,67,142,51,174,67,164,197,186,67,66,3,174,67,98,84,165,187,67,46,128,173,67,92,96,188,67,114,122,172,67,28,180,188,67,78,79,171,67,98,116,211,188,67,98,223,170,67,116,218,188,67,78,122,170,67,132,217,188,67,38,51,169,67,98,124,216,188,
        67,22,196,167,67,100,211,188,67,62,142,167,67,12,160,188,67,14,207,166,67,98,12,129,188,67,194,91,166,67,236,98,188,67,170,248,165,67,20,93,188,67,214,242,165,67,98,68,87,188,67,2,237,165,67,188,38,188,67,150,16,166,67,60,241,187,67,226,65,166,67,98,
        196,187,187,67,46,115,166,67,84,80,187,67,174,185,166,67,132,2,187,67,138,222,166,67,98,172,125,186,67,122,29,167,67,188,102,186,67,130,33,167,67,4,140,185,67,138,32,167,67,98,60,203,184,67,130,31,167,67,12,141,184,67,226,22,167,67,228,35,184,67,186,
        237,166,67,98,52,15,183,67,86,129,166,67,148,60,182,67,250,202,165,67,196,197,181,67,226,224,164,67,98,140,133,181,67,90,98,164,67,12,126,181,67,98,65,164,67,12,126,181,67,134,165,163,67,98,12,126,181,67,214,5,163,67,92,131,181,67,238,239,162,67,140,
        190,181,67,162,155,162,67,98,4,226,181,67,34,105,162,67,124,41,182,67,246,39,162,67,108,93,182,67,214,10,162,67,98,124,179,182,67,138,218,161,67,36,210,182,67,226,213,161,67,252,185,183,67,226,213,161,67,98,108,219,184,67,226,213,161,67,124,96,185,67,
        130,248,161,67,212,136,186,67,254,144,162,67,98,92,176,188,67,194,172,163,67,116,14,190,67,222,82,166,67,116,14,190,67,90,99,169,67,98,116,14,190,67,178,21,171,67,60,174,189,67,214,182,172,67,164,11,189,67,210,197,173,67,98,228,7,188,67,146,118,175,67,
        36,155,185,67,14,52,176,67,164,114,183,67,66,123,175,67,98,204,78,182,67,166,25,175,67,148,191,180,67,230,26,174,67,244,144,179,67,38,1,173,67,98,156,62,177,67,210,215,170,67,108,153,175,67,38,179,167,67,52,242,174,67,150,45,164,67,98,156,191,174,67,
        178,28,163,67,116,179,174,67,198,136,160,67,20,220,174,67,46,122,159,67,98,76,27,175,67,6,213,157,67,244,161,175,67,126,112,156,67,188,124,176,67,226,42,155,67,98,60,242,176,67,18,124,154,67,156,93,178,67,138,17,153,67,20,7,179,67,26,162,152,67,108,180,
        138,179,67,138,75,152,67,108,20,7,179,67,34,52,152,67,98,156,6,178,67,130,6,152,67,108,20,177,67,14,243,151,67,132,203,174,67,10,221,151,67,98,212,76,171,67,94,187,151,67,84,160,168,67,170,109,151,67,228,206,166,67,230,244,150,67,108,196,48,166,67,226,
        203,150,67,108,164,222,164,67,2,191,151,67,108,124,140,163,67,30,178,152,67,108,108,144,163,67,246,159,153,67,98,244,152,163,67,210,162,155,67,132,169,163,67,170,37,156,67,132,2,164,67,14,41,157,67,98,196,114,165,67,30,90,161,67,4,217,167,67,182,234,
        165,67,12,89,170,67,210,55,169,67,98,52,242,171,67,18,84,171,67,228,24,174,67,154,146,173,67,228,242,175,67,194,15,175,67,98,100,48,177,67,18,15,176,67,12,99,179,67,202,132,177,67,132,80,180,67,26,246,177,67,98,244,66,181,67,198,105,178,67,244,130,182,
        67,222,208,178,67,212,111,183,67,158,247,178,67,98,244,117,184,67,126,34,179,67,12,109,186,67,210,18,179,67,228,70,187,67,6,217,178,67,99,109,176,35,195,66,146,196,176,67,98,176,52,210,66,142,68,176,67,176,88,225,66,222,175,174,67,16,44,245,66,62,139,
        171,67,98,80,45,249,66,182,232,170,67,232,164,2,67,130,202,168,67,88,201,2,67,254,172,168,67,98,232,210,2,67,54,165,168,67,120,231,2,67,170,20,168,67,232,246,2,67,190,107,167,67,108,248,18,3,67,150,56,166,67,108,200,34,1,67,78,164,165,67,98,232,17,0,
        67,194,82,165,67,208,57,254,66,90,13,165,67,176,4,254,66,18,10,165,67,98,176,207,253,66,190,6,165,67,144,124,251,66,46,81,165,67,112,218,248,66,90,175,165,67,98,48,131,240,66,170,217,166,67,240,216,236,66,66,71,167,67,144,219,234,66,210,81,167,67,98,
        16,169,232,66,126,93,167,67,240,220,231,66,10,66,167,67,80,78,229,66,130,146,166,67,108,112,116,227,66,118,19,166,67,108,208,49,226,66,30,165,166,67,98,16,214,217,66,26,107,170,67,80,103,204,66,206,11,174,67,80,242,188,66,94,180,176,67,98,16,36,188,66,
        218,215,176,67,208,40,188,66,90,216,176,67,16,87,190,66,114,217,176,67,98,80,142,191,66,122,218,176,67,48,183,193,66,182,208,176,67,176,35,195,66,142,196,176,67,99,109,152,174,13,67,22,21,171,67,98,232,23,14,67,126,163,170,67,72,227,14,67,66,169,169,
        67,120,114,15,67,10,233,168,67,98,40,10,17,67,186,197,166,67,8,9,17,67,86,198,166,67,56,66,19,67,118,170,166,67,98,216,9,20,67,178,160,166,67,136,102,21,67,102,137,166,67,24,73,22,67,186,118,166,67,98,56,5,24,67,30,82,166,67,216,166,24,67,110,89,166,
        67,184,73,25,67,126,153,166,67,98,24,131,25,67,14,176,166,67,88,94,26,67,250,59,167,67,248,48,27,67,122,208,167,67,98,152,3,28,67,246,100,168,67,248,92,29,67,146,68,169,67,120,48,30,67,102,193,169,67,98,24,242,31,67,190,202,170,67,56,110,31,67,18,199,
        170,67,88,211,34,67,130,226,169,67,98,184,34,38,67,174,3,169,67,248,76,38,67,166,244,168,67,248,76,38,67,86,166,168,67,98,248,76,38,67,242,129,168,67,184,181,37,67,78,126,167,67,216,252,36,67,94,101,166,67,98,248,67,36,67,114,76,165,67,104,144,35,67,
        122,53,164,67,232,109,35,67,118,249,163,67,108,24,47,35,67,86,140,163,67,108,120,170,37,67,218,28,162,67,98,232,7,39,67,190,82,161,67,88,52,40,67,210,165,160,67,40,70,40,67,142,156,160,67,98,8,88,40,67,78,147,160,67,184,130,41,67,246,178,160,67,8,222,
        42,67,234,226,160,67,98,72,206,45,67,202,74,161,67,200,34,49,67,90,174,161,67,72,87,50,67,98,194,161,67,108,216,53,51,67,210,208,161,67,108,136,230,51,67,186,20,161,67,98,216,127,52,67,138,113,160,67,200,54,54,67,250,55,158,67,72,34,54,67,246,46,158,
        67,98,120,172,53,67,98,251,157,67,200,143,47,67,50,44,156,67,120,191,45,67,114,179,155,67,108,120,214,44,67,222,118,155,67,108,216,219,44,67,206,129,153,67,108,56,225,44,67,186,140,151,67,108,24,29,44,67,230,126,151,67,98,56,138,42,67,126,98,151,67,232,
        125,38,67,18,242,150,67,248,228,36,67,98,182,150,67,98,8,252,35,67,102,148,150,67,232,53,35,67,106,124,150,67,152,44,35,67,18,129,150,67,98,56,35,35,67,194,133,150,67,248,67,35,67,58,240,150,67,88,117,35,67,186,109,151,67,98,120,39,36,67,154,50,153,67,
        152,26,36,67,134,213,153,67,216,36,35,67,238,101,155,67,98,168,200,33,67,86,157,157,67,232,40,31,67,174,102,159,67,8,139,27,67,174,146,160,67,98,232,196,25,67,218,37,161,67,168,39,25,67,54,77,161,67,8,101,23,67,162,156,161,67,98,24,34,21,67,162,2,162,
        67,8,4,19,67,254,38,162,67,216,18,16,67,74,26,162,67,98,40,73,13,67,62,14,162,67,136,28,12,67,86,239,161,67,168,52,10,67,218,127,161,67,98,88,170,6,67,190,176,160,67,72,159,3,67,250,124,159,67,216,164,1,67,254,29,158,67,108,24,80,1,67,66,227,157,67,108,
        48,72,255,66,90,180,158,67,98,240,0,247,66,18,186,160,67,208,12,238,66,242,49,162,67,240,150,228,66,222,20,163,67,98,208,45,227,66,182,54,163,67,48,6,226,66,186,88,163,67,48,6,226,66,118,96,163,67,98,48,6,226,66,226,120,163,67,80,169,229,66,162,180,164,
        67,240,232,231,66,118,95,165,67,98,144,72,233,66,202,199,165,67,48,80,234,66,162,3,166,67,80,188,234,66,162,3,166,67,98,208,114,235,66,162,3,166,67,112,202,242,66,122,18,165,67,240,131,249,66,42,30,164,67,98,80,201,251,66,170,203,163,67,16,204,253,66,
        78,136,163,67,208,251,253,66,126,136,163,67,98,176,43,254,66,126,136,163,67,168,136,0,67,194,244,163,67,40,54,2,67,178,120,164,67,98,184,252,5,67,158,161,165,67,136,204,5,67,2,119,165,67,216,140,5,67,190,107,167,67,98,88,116,5,67,250,43,168,67,200,83,
        5,67,182,79,169,67,104,68,5,67,10,244,169,67,108,104,40,5,67,206,30,171,67,108,88,118,6,67,210,75,171,67,98,8,98,8,67,22,142,171,67,40,122,11,67,114,219,171,67,24,107,12,67,182,224,171,67,98,136,233,12,67,134,227,171,67,8,246,12,67,162,219,171,67,56,
        174,13,67,22,21,171,67,99,109,180,224,226,67,38,28,171,67,98,20,79,226,67,150,210,170,67,188,175,220,67,66,16,168,67,36,98,214,67,138,250,164,67,98,148,20,208,67,206,228,161,67,180,229,202,67,246,88,159,67,148,221,202,67,2,82,159,67,98,124,213,202,67,
        10,75,159,67,228,57,205,67,54,159,158,67,140,46,208,67,46,212,157,67,98,52,35,211,67,42,9,157,67,60,142,213,67,146,93,156,67,36,142,213,67,230,86,156,67,98,36,142,213,67,62,80,156,67,244,138,209,67,150,225,154,67,180,163,204,67,46,40,153,67,98,124,188,
        199,67,198,110,151,67,68,177,195,67,242,0,150,67,60,167,195,67,58,251,149,67,98,36,157,195,67,130,245,149,67,76,225,196,67,110,94,149,67,116,119,198,67,118,171,148,67,98,148,13,200,67,126,248,147,67,140,117,201,67,118,89,147,67,92,151,201,67,18,74,147,
        67,98,132,210,201,67,34,47,147,67,244,189,201,67,70,38,147,67,188,116,199,67,118,94,146,67,98,84,38,198,67,70,236,145,67,60,18,197,67,74,146,145,67,52,15,197,67,130,150,145,67,98,44,12,197,67,198,154,145,67,4,219,196,67,234,224,145,67,244,161,196,67,
        122,50,146,67,98,212,247,194,67,82,147,148,67,76,215,191,67,130,97,150,67,212,11,188,67,146,39,151,67,98,252,57,187,67,90,82,151,67,180,72,184,67,190,173,151,67,196,127,183,67,202,180,151,67,98,140,101,183,67,210,181,151,67,4,90,183,67,154,186,151,67,
        44,102,183,67,174,191,151,67,98,84,114,183,67,198,196,151,67,196,61,187,67,2,102,152,67,236,212,191,67,6,38,153,67,98,28,108,196,67,14,230,153,67,60,48,200,67,190,133,154,67,100,51,200,67,242,136,154,67,98,164,54,200,67,42,140,154,67,220,91,197,67,94,
        53,155,67,132,219,193,67,6,1,156,67,98,52,91,190,67,174,204,156,67,252,125,187,67,18,120,157,67,36,126,187,67,230,125,157,67,98,36,126,187,67,194,137,157,67,4,179,227,67,14,162,171,67,76,212,227,67,250,161,171,67,98,244,223,227,67,250,161,171,67,84,114,
        227,67,186,101,171,67,180,224,226,67,42,28,171,67,99,109,76,245,140,67,70,74,170,67,98,140,123,141,67,126,135,169,67,156,153,141,67,134,20,169,67,212,153,141,67,182,213,167,67,98,212,153,141,67,30,217,166,67,44,147,141,67,146,161,166,67,116,95,141,67,
        146,249,165,67,98,60,63,141,67,246,144,165,67,212,32,141,67,78,55,165,67,220,27,141,67,82,50,165,67,98,220,22,141,67,86,45,165,67,20,164,139,67,206,92,165,67,228,227,137,67,202,155,165,67,98,196,39,135,67,42,254,165,67,220,195,134,67,134,16,166,67,188,
        30,135,67,26,30,166,67,98,92,2,136,67,34,64,166,67,204,169,136,67,162,211,166,67,132,239,136,67,122,183,167,67,98,236,2,137,67,246,246,167,67,108,41,137,67,198,35,168,67,252,145,137,67,150,116,168,67,98,148,126,139,67,70,241,169,67,76,139,140,67,122,
        183,170,67,180,156,140,67,246,178,170,67,98,188,167,140,67,14,176,170,67,148,207,140,67,254,128,170,67,76,245,140,67,70,74,170,67,99,109,212,113,186,67,42,185,166,67,98,4,4,187,67,150,154,166,67,100,88,187,67,174,108,166,67,52,240,187,67,50,233,165,67,
        108,204,66,188,67,158,161,165,67,108,28,7,188,67,194,59,165,67,98,92,117,187,67,254,66,164,67,4,55,186,67,74,61,163,67,236,39,185,67,74,223,162,67,98,188,127,184,67,250,164,162,67,212,111,183,67,210,144,162,67,212,221,182,67,218,179,162,67,98,60,83,182,
        67,26,213,162,67,92,42,182,67,34,24,163,67,4,45,182,67,178,213,163,67,98,132,47,182,67,2,138,164,67,244,125,182,67,14,36,165,67,20,40,183,67,58,195,165,67,98,180,39,184,67,86,178,166,67,228,41,185,67,202,253,166,67,212,113,186,67,42,185,166,67,99,109,
        236,245,144,67,50,148,162,67,98,236,245,144,67,42,33,162,67,100,244,144,67,34,30,162,67,44,148,144,67,214,218,161,67,98,84,61,142,67,218,55,160,67,132,29,140,67,26,167,158,67,116,24,140,67,6,141,158,67,98,60,12,140,67,50,78,158,67,116,84,142,67,222,75,
        157,67,108,80,145,67,78,62,156,67,98,188,157,145,67,6,35,156,67,84,162,145,67,158,27,156,67,156,222,145,67,6,88,155,67,98,236,0,146,67,218,232,154,67,252,84,146,67,206,216,153,67,124,153,146,67,110,251,152,67,98,244,221,146,67,18,30,152,67,76,19,147,
        67,138,101,151,67,244,15,147,67,90,97,151,67,98,164,12,147,67,50,93,151,67,156,148,146,67,134,104,151,67,52,5,146,67,174,122,151,67,98,4,169,144,67,162,166,151,67,220,69,144,67,50,175,151,67,204,123,142,67,186,200,151,67,108,84,245,140,67,126,222,151,
        67,108,20,32,140,67,38,173,152,67,98,140,46,139,67,70,151,153,67,180,142,138,67,82,22,154,67,236,157,137,67,170,171,154,67,98,180,83,136,67,118,120,155,67,84,243,134,67,134,18,156,67,44,164,133,67,146,104,156,67,108,204,30,133,67,206,138,156,67,108,116,
        250,138,67,54,201,159,67,98,68,51,142,67,242,145,161,67,68,221,144,67,246,7,163,67,252,229,144,67,94,8,163,67,98,188,238,144,67,94,8,163,67,228,245,144,67,122,212,162,67,228,245,144,67,46,148,162,67,99,109,88,86,74,67,78,22,162,67,98,40,8,83,67,34,111,
        161,67,200,161,90,67,142,8,159,67,152,142,95,67,86,98,155,67,98,136,7,96,67,186,8,155,67,24,100,96,67,66,188,154,67,72,92,96,67,102,184,154,67,98,56,54,96,67,138,165,154,67,136,171,93,67,106,25,154,67,24,122,93,67,106,25,154,67,98,216,90,93,67,106,25,
        154,67,136,42,93,67,218,46,154,67,200,14,93,67,18,73,154,67,98,168,155,92,67,142,181,154,67,8,194,90,67,134,215,155,67,8,110,89,67,174,129,156,67,98,168,48,84,67,230,32,159,67,216,78,77,67,78,141,160,67,216,221,69,67,146,141,160,67,98,104,130,65,67,146,
        141,160,67,40,221,61,67,186,36,160,67,104,204,57,67,246,49,159,67,98,88,25,57,67,46,8,159,67,168,126,56,67,6,230,158,67,168,116,56,67,6,230,158,67,98,216,91,56,67,6,230,158,67,24,76,55,67,58,100,160,67,216,94,55,67,182,108,160,67,98,40,103,55,67,114,
        112,160,67,72,224,55,67,2,144,160,67,24,108,56,67,230,178,160,67,98,104,188,58,67,178,70,161,67,120,106,62,67,170,215,161,67,72,28,65,67,50,9,162,67,98,24,203,65,67,194,21,162,67,168,137,66,67,130,35,162,67,248,195,66,67,202,39,162,67,98,24,239,67,67,
        190,61,162,67,136,225,72,67,86,50,162,67,120,86,74,67,86,22,162,67,99,109,8,215,105,67,110,131,160,67,98,200,174,112,67,246,20,160,67,232,77,118,67,166,183,159,67,24,85,118,67,14,180,159,67,98,88,92,118,67,134,176,159,67,40,158,116,67,222,71,159,67,200,
        117,114,67,162,203,158,67,98,104,77,112,67,102,79,158,67,120,169,108,67,214,124,157,67,184,94,106,67,190,247,156,67,108,8,52,102,67,190,5,156,67,108,248,22,101,67,74,220,156,67,98,120,5,99,67,206,106,158,67,88,250,96,67,246,154,159,67,24,86,94,67,146,
        201,160,67,98,104,181,93,67,118,17,161,67,184,61,93,67,70,76,161,67,248,75,93,67,70,76,161,67,98,88,90,93,67,70,76,161,67,56,255,98,67,230,241,160,67,248,214,105,67,110,131,160,67,99,109,216,99,18,67,102,213,160,67,98,88,183,18,67,50,202,160,67,232,168,
        18,67,66,191,160,67,136,207,17,67,190,100,160,67,98,168,117,14,67,130,255,158,67,72,69,12,67,226,14,157,67,56,166,11,67,194,238,154,67,98,56,97,11,67,242,2,154,67,168,137,11,67,110,90,152,67,104,249,11,67,202,131,151,67,98,152,1,13,67,138,136,149,67,
        8,187,15,67,254,143,147,67,72,226,18,67,34,132,146,67,108,136,231,19,67,106,45,146,67,108,216,33,19,67,98,204,145,67,108,8,92,18,67,90,107,145,67,108,56,136,17,67,150,183,145,67,98,200,187,15,67,70,93,146,67,184,214,13,67,206,90,147,67,72,141,12,67,250,
        81,148,67,98,232,172,11,67,70,250,148,67,168,26,10,67,22,234,150,67,136,190,9,67,210,199,151,67,98,152,37,9,67,26,56,153,67,168,71,9,67,186,226,154,67,168,27,10,67,54,85,156,67,98,168,241,10,67,10,203,157,67,104,81,13,67,206,202,159,67,40,50,15,67,214,
        157,160,67,98,56,193,15,67,166,220,160,67,24,230,15,67,94,226,160,67,200,234,16,67,26,226,160,67,98,72,135,17,67,26,226,160,67,232,48,18,67,70,220,160,67,200,99,18,67,102,213,160,67,99,109,80,100,208,66,182,25,160,67,98,240,160,212,66,46,239,159,67,112,
        250,218,66,150,128,159,67,80,16,223,66,42,26,159,67,98,16,0,241,66,122,88,157,67,240,164,253,66,122,214,153,67,136,130,3,67,122,4,148,67,98,168,165,5,67,42,93,145,67,248,39,9,67,226,136,139,67,216,37,13,67,162,1,132,67,98,184,91,16,67,92,231,123,67,184,
        138,18,67,172,167,114,67,24,1,20,67,44,248,106,67,98,56,63,20,67,252,177,105,67,248,196,20,67,180,22,103,67,88,42,21,67,100,45,101,67,98,184,14,22,67,36,223,96,67,24,85,22,67,180,99,94,67,8,84,22,67,52,171,90,67,98,8,84,22,67,92,73,87,67,200,15,22,67,
        60,2,86,67,136,213,20,67,108,105,83,67,98,56,66,19,67,84,20,80,67,88,161,17,67,68,112,78,67,104,46,14,67,52,181,76,67,98,120,248,11,67,52,153,75,67,40,17,9,67,156,192,74,67,72,200,5,67,156,66,74,67,98,88,102,3,67,60,231,73,67,112,114,251,66,132,252,73,
        67,80,57,245,66,124,108,74,67,98,240,200,237,66,100,242,74,67,112,205,230,66,212,149,75,67,112,222,228,66,84,235,75,67,108,144,47,227,66,188,53,76,67,108,208,253,226,66,84,15,84,67,98,208,214,226,66,12,67,90,67,48,175,226,66,244,56,92,67,48,66,226,66,
        52,102,93,67,98,48,246,225,66,236,55,94,67,176,66,225,66,236,134,96,67,80,179,224,66,140,135,98,67,98,112,56,222,66,140,99,107,67,240,54,216,66,220,4,120,67,144,14,208,66,230,195,130,67,98,176,103,204,66,22,202,133,67,144,95,197,66,222,106,139,67,176,
        60,195,66,134,6,141,67,98,16,121,189,66,118,93,145,67,240,123,184,66,98,46,147,67,208,91,175,66,26,67,148,67,98,112,117,173,66,182,124,148,67,16,12,172,66,18,149,148,67,112,25,169,66,18,175,148,67,98,240,75,161,66,230,243,148,67,208,11,156,66,170,36,
        149,67,80,255,155,66,202,40,149,67,98,48,248,155,66,18,43,149,67,208,100,155,66,22,119,149,67,240,183,154,66,158,209,149,67,98,48,148,153,66,118,106,150,67,176,119,153,66,246,138,150,67,48,41,153,66,10,153,151,67,98,176,250,152,66,2,57,152,67,240,237,
        152,66,78,7,153,67,112,12,153,66,130,99,153,67,98,176,213,153,66,146,190,155,67,240,94,160,66,74,242,157,67,80,173,169,66,126,253,158,67,98,16,76,174,66,46,130,159,67,80,2,181,66,254,246,159,67,176,121,186,66,226,33,160,67,98,48,29,188,66,190,46,160,
        67,176,230,189,66,42,61,160,67,112,114,190,66,242,65,160,67,98,176,182,192,66,198,85,160,67,48,50,205,66,186,57,160,67,80,100,208,66,182,25,160,67,99,109,144,241,190,66,242,193,158,67,98,208,241,179,66,138,151,158,67,240,237,168,66,154,135,157,67,112,
        0,164,66,218,40,156,67,98,176,58,162,66,170,170,155,67,16,37,160,66,130,164,154,67,144,77,159,66,218,217,153,67,98,240,113,158,66,94,11,153,67,112,131,158,66,182,238,151,67,112,115,159,66,58,105,151,67,98,240,211,160,66,106,165,150,67,208,78,162,66,174,
        109,150,67,240,199,166,66,78,85,150,67,98,16,53,175,66,98,39,150,67,80,144,181,66,6,107,149,67,144,131,187,66,250,238,147,67,98,80,157,190,66,238,40,147,67,16,175,192,66,210,100,146,67,176,255,194,66,170,41,145,67,98,144,46,199,66,26,240,142,67,176,130,
        201,66,126,79,141,67,240,211,206,66,22,229,136,67,98,48,255,208,66,246,23,135,67,144,40,211,66,54,87,133,67,112,161,211,66,214,255,132,67,98,208,231,212,66,242,19,132,67,80,112,219,66,44,168,124,67,48,17,222,66,28,161,119,67,98,80,22,226,66,180,240,111,
        67,176,41,229,66,220,125,104,67,208,165,230,66,20,218,98,67,98,176,7,231,66,116,102,97,67,144,143,231,66,12,157,95,67,176,211,231,66,156,225,94,67,98,112,167,232,66,76,155,92,67,80,247,232,66,180,200,89,67,176,248,232,66,4,137,84,67,108,176,248,232,66,
        52,122,79,67,108,176,225,233,66,52,13,79,67,98,144,212,235,66,228,35,78,67,16,212,238,66,100,176,77,67,112,142,244,66,20,114,77,67,98,40,100,1,67,76,215,76,67,72,243,2,67,100,212,76,67,72,28,6,67,12,92,77,67,98,40,22,8,67,228,176,77,67,120,27,11,67,36,
        127,78,67,200,56,12,67,84,253,78,67,98,200,253,14,67,228,54,80,67,104,115,17,67,236,190,82,67,216,148,18,67,244,136,85,67,98,104,206,19,67,148,142,88,67,200,169,19,67,116,40,94,67,216,49,18,67,76,174,100,67,98,40,248,17,67,156,174,101,67,232,151,17,67,
        188,148,103,67,248,91,17,67,148,230,104,67,98,8,238,15,67,180,244,112,67,184,252,13,67,76,24,121,67,24,1,9,67,106,243,133,67,98,136,114,7,67,150,227,136,67,216,241,3,67,22,233,142,67,152,203,2,67,250,159,144,67,98,56,255,0,67,138,78,147,67,112,43,253,
        66,170,19,150,67,240,149,249,66,142,132,151,67,98,240,161,240,66,250,29,155,67,144,217,226,66,198,166,157,67,144,40,212,66,98,102,158,67,98,144,8,206,66,66,182,158,67,208,122,197,66,42,219,158,67,240,239,190,66,242,193,158,67,99,109,112,168,207,66,154,
        223,157,67,98,112,87,216,66,194,121,157,67,144,100,225,66,178,143,156,67,80,72,231,66,170,124,155,67,98,48,61,241,66,194,171,153,67,48,1,249,66,98,229,150,67,176,79,255,66,130,230,146,67,98,56,220,1,67,114,27,144,67,56,79,7,67,34,170,134,67,200,35,11,
        67,52,0,126,67,98,24,147,12,67,68,66,120,67,8,193,14,67,84,178,110,67,24,98,15,67,28,98,107,67,98,104,153,15,67,220,62,106,67,8,11,16,67,236,2,104,67,152,94,16,67,36,107,102,67,98,152,139,18,67,140,205,91,67,136,113,18,67,4,96,87,67,120,240,15,67,12,
        176,83,67,98,232,133,14,67,20,154,81,67,24,44,11,67,108,208,79,67,120,48,7,67,212,4,79,67,98,56,213,3,67,76,89,78,67,240,199,254,66,164,62,78,67,176,9,248,66,188,193,78,67,98,208,171,245,66,188,239,78,67,48,252,241,66,228,53,79,67,144,216,239,66,156,
        93,79,67,108,16,245,235,66,196,165,79,67,108,208,240,235,66,228,29,85,67,98,208,240,235,66,244,31,88,67,80,204,235,66,124,27,91,67,112,164,235,66,148,190,91,67,98,48,18,233,66,60,59,102,67,240,38,231,66,116,127,107,67,176,135,226,66,252,193,116,67,98,
        144,2,219,66,78,233,129,67,16,70,203,66,206,161,142,67,112,18,198,66,22,98,145,67,98,112,237,191,66,14,162,148,67,48,180,180,66,226,192,150,67,240,35,168,66,194,9,151,67,98,144,86,164,66,206,31,151,67,144,227,163,66,166,46,151,67,16,177,162,66,178,189,
        151,67,98,208,26,162,66,198,3,152,67,240,251,161,66,54,53,152,67,48,255,161,66,170,219,152,67,98,80,3,162,66,182,134,153,67,16,39,162,66,38,187,153,67,80,244,162,66,46,57,154,67,98,80,169,164,66,146,69,155,67,112,209,167,66,126,65,156,67,144,241,170,
        66,186,182,156,67,98,80,222,173,66,110,36,157,67,240,207,182,66,242,197,157,67,112,248,188,66,70,252,157,67,98,144,92,192,66,46,26,158,67,112,54,204,66,2,8,158,67,48,168,207,66,154,223,157,67,99,109,168,29,24,67,62,22,160,67,98,72,151,24,67,130,247,159,
        67,152,99,25,67,202,188,159,67,184,227,25,67,190,147,159,67,108,184,204,26,67,26,73,159,67,108,40,164,25,67,34,27,159,67,98,88,122,23,67,78,197,158,67,152,220,21,67,146,68,158,67,152,140,20,67,146,133,157,67,98,104,123,19,67,70,234,156,67,216,9,19,67,
        202,150,156,67,40,141,18,67,122,13,156,67,98,184,230,15,67,202,34,153,67,200,114,18,67,238,194,149,67,152,71,24,67,178,118,148,67,108,168,63,25,67,126,63,148,67,108,120,203,23,67,90,186,147,67,108,56,87,22,67,58,53,147,67,108,232,124,21,67,98,110,147,
        67,98,248,194,18,67,26,37,148,67,24,59,16,67,54,182,149,67,232,32,15,67,86,94,151,67,98,72,233,12,67,118,179,154,67,88,207,15,67,118,93,158,67,120,250,21,67,18,7,160,67,98,88,137,22,67,150,45,160,67,40,13,23,67,78,77,160,67,104,31,23,67,146,77,160,67,
        98,168,49,23,67,146,77,160,67,8,164,23,67,234,52,160,67,152,29,24,67,50,22,160,67,99,109,88,200,7,67,218,197,158,67,98,88,25,7,67,66,14,158,67,104,101,6,67,22,22,157,67,104,7,6,67,158,90,156,67,108,72,218,5,67,154,0,156,67,108,168,18,5,67,214,1,156,67,
        98,40,86,4,67,250,2,156,67,56,66,4,67,234,8,156,67,216,170,3,67,58,108,156,67,108,152,10,3,67,106,213,156,67,108,88,146,3,67,142,57,157,67,98,56,149,4,67,126,248,157,67,88,112,5,67,130,108,158,67,136,233,6,67,70,254,158,67,98,8,186,7,67,214,78,159,67,
        72,106,8,67,242,141,159,67,40,113,8,67,122,138,159,67,98,8,120,8,67,14,135,159,67,40,44,8,67,142,46,159,67,88,200,7,67,234,197,158,67,99,109,120,101,29,67,162,22,158,67,98,120,98,30,67,158,138,157,67,56,3,32,67,126,51,156,67,56,3,32,67,62,239,155,67,
        98,56,3,32,67,14,229,155,67,120,159,31,67,98,245,155,67,136,37,31,67,142,19,156,67,98,184,112,28,67,254,190,156,67,136,251,24,67,146,14,156,67,40,234,23,67,110,162,154,67,98,56,109,23,67,18,252,153,67,232,122,23,67,126,21,153,67,104,10,24,67,214,130,
        152,67,98,216,145,24,67,90,248,151,67,120,150,25,67,74,121,151,67,248,176,26,67,246,55,151,67,98,184,112,27,67,162,11,151,67,40,224,27,67,158,0,151,67,72,222,28,67,218,0,151,67,98,216,67,30,67,218,0,151,67,168,60,31,67,186,45,151,67,40,98,32,67,250,161,
        151,67,98,136,12,33,67,114,229,151,67,184,14,33,67,178,229,151,67,232,246,32,67,78,178,151,67,98,168,233,32,67,162,149,151,67,184,189,32,67,34,45,151,67,88,149,32,67,26,202,150,67,98,232,108,32,67,14,103,150,67,200,54,32,67,10,11,150,67,248,28,32,67,
        154,253,149,67,98,120,222,31,67,6,221,149,67,24,61,28,67,50,11,149,67,248,178,27,67,114,254,148,67,98,72,18,27,67,162,239,148,67,72,204,24,67,62,88,149,67,72,158,23,67,54,186,149,67,98,184,249,21,67,162,66,150,67,56,182,20,67,6,16,151,67,8,237,19,67,
        114,18,152,67,98,232,152,19,67,114,126,152,67,168,137,19,67,74,185,152,67,88,135,19,67,82,154,153,67,98,8,133,19,67,22,123,154,67,168,147,19,67,194,184,154,67,184,231,19,67,114,50,155,67,98,168,238,20,67,30,175,156,67,200,166,23,67,210,231,157,67,152,
        199,26,67,250,72,158,67,98,136,83,28,67,6,121,158,67,232,194,28,67,166,112,158,67,120,101,29,67,166,22,158,67,99,109,136,240,72,67,166,37,158,67,98,184,66,78,67,98,191,157,67,168,7,83,67,6,135,156,67,184,137,86,67,10,171,154,67,98,56,133,87,67,186,37,
        154,67,168,40,89,67,82,26,153,67,24,16,89,67,226,14,153,67,98,184,10,89,67,70,12,153,67,184,4,88,67,114,208,152,67,56,202,86,67,210,137,152,67,108,72,142,84,67,114,9,152,67,108,72,37,84,67,126,77,152,67,98,88,121,81,67,218,8,154,67,120,26,77,67,230,81,
        155,67,104,174,72,67,150,172,155,67,98,216,173,70,67,162,213,155,67,152,15,67,67,134,188,155,67,216,32,65,67,38,120,155,67,98,24,118,61,67,110,246,154,67,136,71,58,67,190,236,153,67,200,170,55,67,50,98,152,67,108,232,238,54,67,86,243,151,67,108,168,30,
        52,67,34,225,151,67,98,152,146,50,67,42,215,151,67,120,71,49,67,186,209,151,67,8,63,49,67,30,213,151,67,98,152,31,49,67,162,225,151,67,8,106,50,67,170,242,152,67,8,103,51,67,74,157,153,67,98,72,241,54,67,110,0,156,67,216,57,60,67,26,154,157,67,104,62,
        66,67,6,28,158,67,98,120,215,67,67,134,62,158,67,136,97,71,67,158,67,158,67,152,240,72,67,178,37,158,67,99,109,212,96,154,67,226,185,154,67,98,100,147,155,67,242,169,154,67,228,147,156,67,58,151,154,67,220,154,156,67,66,144,154,67,98,180,200,156,67,106,
        98,154,67,196,36,154,67,30,71,152,67,172,67,152,67,166,25,151,67,98,68,198,151,67,18,203,150,67,36,81,151,67,182,138,150,67,108,63,151,67,170,138,150,67,98,180,45,151,67,170,138,150,67,196,151,150,67,14,170,150,67,60,242,149,67,138,208,150,67,108,68,
        197,148,67,142,22,151,67,108,68,20,149,67,198,127,151,67,98,180,63,149,67,166,185,151,67,68,218,149,67,250,145,152,67,180,107,150,67,134,96,153,67,108,52,116,151,67,10,216,154,67,108,220,211,151,67,254,214,154,67,98,116,8,152,67,254,214,154,67,68,46,
        153,67,90,201,154,67,212,96,154,67,110,185,154,67,99,109,56,238,73,67,86,251,153,67,98,136,23,77,67,198,131,153,67,40,223,79,67,146,149,152,67,168,191,81,67,82,93,151,67,98,8,130,82,67,2,223,150,67,248,205,83,67,154,190,149,67,8,207,83,67,34,147,149,
        67,98,8,207,83,67,202,130,149,67,56,123,83,67,58,139,149,67,8,220,82,67,118,171,149,67,98,152,109,80,67,118,41,150,67,152,131,75,67,46,214,150,67,104,4,72,67,134,40,151,67,98,24,146,69,67,30,98,151,67,136,174,64,67,126,174,151,67,56,166,61,67,118,202,
        151,67,108,104,196,58,67,14,229,151,67,108,168,148,59,67,74,65,152,67,98,200,166,61,67,22,44,153,67,24,49,64,67,254,211,153,67,248,195,66,67,46,28,154,67,98,152,205,68,67,82,85,154,67,24,238,71,67,254,70,154,67,56,238,73,67,86,251,153,67,99,109,20,74,
        157,67,114,29,152,67,108,100,99,157,67,246,179,151,67,108,140,96,156,67,194,84,150,67,98,52,210,155,67,154,147,149,67,252,82,155,67,210,249,148,67,228,69,155,67,6,255,148,67,98,196,56,155,67,58,4,149,67,180,193,154,67,138,54,149,67,68,61,154,67,214,110,
        149,67,98,148,100,153,67,250,202,149,67,116,80,153,67,6,216,149,67,4,116,153,67,78,241,149,67,98,188,137,153,67,194,0,150,67,60,28,154,67,90,97,150,67,132,185,154,67,242,199,150,67,98,204,86,155,67,138,46,151,67,156,34,156,67,22,189,151,67,108,126,156,
        67,182,4,152,67,98,68,218,156,67,82,76,152,67,244,39,157,67,238,134,152,67,20,43,157,67,238,134,152,67,98,52,46,157,67,238,134,152,67,44,60,157,67,114,87,152,67,20,74,157,67,114,29,152,67,99,109,88,132,88,67,178,83,151,67,98,168,247,89,67,30,43,151,67,
        8,55,91,67,238,253,150,67,24,74,91,67,78,239,150,67,98,8,245,91,67,198,107,150,67,152,73,93,67,226,16,147,67,216,210,92,67,226,16,147,67,98,184,192,92,67,226,16,147,67,24,119,92,67,198,38,147,67,8,47,92,67,134,65,147,67,98,24,104,91,67,86,139,147,67,
        168,73,89,67,58,47,148,67,120,102,88,67,38,102,148,67,98,216,185,87,67,222,143,148,67,56,150,87,67,106,163,148,67,8,113,87,67,154,236,148,67,98,40,32,87,67,250,139,149,67,8,113,86,67,22,106,150,67,88,201,85,67,254,5,151,67,98,152,109,85,67,86,91,151,
        67,120,34,85,67,102,165,151,67,120,34,85,67,142,170,151,67,98,120,34,85,67,166,175,151,67,88,77,85,67,210,174,151,67,200,129,85,67,178,168,151,67,98,56,182,85,67,142,162,151,67,232,16,87,67,70,124,151,67,88,132,88,67,178,83,151,67,99,109,200,222,98,67,
        42,54,150,67,108,152,28,100,67,134,19,150,67,108,104,78,100,67,226,175,149,67,98,88,150,100,67,218,31,149,67,248,218,100,67,190,241,147,67,24,219,100,67,2,68,147,67,108,24,219,100,67,230,173,146,67,108,104,51,99,67,30,63,146,67,108,200,139,97,67,90,208,
        145,67,108,200,139,97,67,6,186,146,67,98,200,139,97,67,86,221,147,67,88,38,97,67,46,100,149,67,136,157,96,67,6,80,150,67,98,232,133,96,67,190,120,150,67,8,117,96,67,126,121,150,67,168,222,98,67,42,54,150,67,99,109,100,170,208,67,134,139,149,67,98,180,
        171,209,67,202,22,149,67,156,236,210,67,206,227,147,67,84,213,211,67,210,131,146,67,98,196,142,212,67,70,107,145,67,36,48,214,67,62,153,142,67,36,96,214,67,226,29,142,67,98,4,209,214,67,202,251,140,67,68,150,214,67,206,76,140,67,252,152,213,67,246,204,
        139,67,98,92,106,213,67,114,181,139,67,116,238,212,67,110,88,139,67,156,133,212,67,70,254,138,67,98,196,28,212,67,30,164,138,67,12,163,211,67,146,66,138,67,36,119,211,67,118,37,138,67,108,68,39,211,67,154,240,137,67,108,52,36,210,67,250,9,138,67,98,36,
        70,208,67,214,56,138,67,4,230,206,67,138,123,138,67,52,172,206,67,50,178,138,67,98,52,164,206,67,198,185,138,67,132,142,206,67,242,51,139,67,20,124,206,67,170,193,139,67,98,124,252,205,67,182,150,143,67,244,251,205,67,214,166,145,67,52,122,206,67,62,
        152,147,67,98,156,243,206,67,202,118,149,67,12,151,207,67,118,8,150,67,116,170,208,67,130,139,149,67,99,109,76,90,159,67,58,236,147,67,108,236,175,159,67,174,116,147,67,108,20,99,159,67,182,29,147,67,98,212,56,159,67,222,237,146,67,204,14,159,67,106,
        190,146,67,180,5,159,67,70,180,146,67,98,148,252,158,67,22,170,146,67,180,146,158,67,218,232,146,67,100,26,158,67,174,63,147,67,98,20,162,157,67,130,150,147,67,124,25,157,67,154,244,147,67,228,234,156,67,198,16,148,67,108,36,150,156,67,6,68,148,67,108,
        28,180,157,67,138,79,148,67,98,100,81,158,67,230,85,148,67,124,221,158,67,10,93,148,67,100,235,158,67,110,95,148,67,98,76,249,158,67,210,97,148,67,52,43,159,67,254,45,148,67,76,90,159,67,58,236,147,67,99,109,136,35,61,67,18,163,147,67,98,248,30,67,67,
        162,106,147,67,72,43,73,67,30,228,146,67,40,70,77,67,50,60,146,67,98,248,141,88,67,194,110,144,67,184,238,95,67,70,15,141,67,200,64,100,67,214,186,135,67,98,40,35,102,67,194,103,133,67,248,161,103,67,62,233,130,67,168,35,105,67,180,40,127,67,98,120,43,
        106,67,236,153,122,67,232,71,107,67,4,235,117,67,184,56,108,67,12,43,114,67,98,136,128,110,67,52,19,105,67,200,164,111,67,116,11,99,67,24,53,112,67,116,29,93,67,98,200,118,112,67,12,106,90,67,24,119,112,67,60,10,90,67,56,57,112,67,164,93,88,67,98,88,
        108,111,67,108,210,82,67,136,205,108,67,220,19,79,67,40,0,104,67,172,157,76,67,98,104,167,98,67,252,223,73,67,40,185,91,67,60,67,73,67,232,145,82,67,228,184,74,67,98,8,29,81,67,84,244,74,67,136,83,79,67,36,51,75,67,40,153,78,67,108,68,75,67,98,232,191,
        77,67,156,88,75,67,168,9,77,67,180,132,75,67,104,157,76,67,116,191,75,67,98,152,64,76,67,204,241,75,67,168,221,75,67,12,27,76,67,168,193,75,67,12,27,76,67,98,232,154,75,67,12,27,76,67,104,144,75,67,84,112,77,67,200,149,75,67,220,176,81,67,98,56,157,75,
        67,100,177,87,67,136,138,75,67,108,178,88,67,248,134,74,67,148,75,96,67,98,216,9,73,67,124,115,107,67,40,165,70,67,156,203,117,67,248,46,67,67,206,4,128,67,98,120,156,64,67,246,210,131,67,120,222,62,67,90,139,133,67,56,69,60,67,102,216,134,67,98,72,252,
        58,67,14,125,135,67,216,23,58,67,178,185,135,67,216,241,56,67,90,186,135,67,98,8,1,56,67,102,187,135,67,24,101,55,67,122,146,135,67,72,223,54,67,214,48,135,67,98,120,200,53,67,94,101,134,67,56,187,53,67,130,44,133,67,40,176,54,67,14,217,130,67,98,168,
        214,55,67,82,13,128,67,8,214,56,67,204,210,123,67,72,6,58,67,132,97,119,67,98,216,139,61,67,212,54,106,67,104,177,62,67,124,141,100,67,232,34,63,67,212,36,94,67,98,88,113,63,67,252,182,89,67,152,253,62,67,212,157,85,67,216,244,61,67,244,114,83,67,98,
        184,199,59,67,12,227,78,67,8,114,55,67,20,243,75,67,40,240,48,67,68,160,74,67,98,24,91,46,67,212,25,74,67,88,107,42,67,188,222,73,67,184,223,39,67,44,24,74,67,98,216,111,35,67,84,124,74,67,184,164,30,67,220,3,75,67,56,178,29,67,252,55,75,67,98,248,251,
        27,67,36,150,75,67,56,108,26,67,244,57,76,67,24,77,26,67,44,156,76,67,98,184,34,26,67,124,33,77,67,216,46,26,67,212,81,79,67,184,94,26,67,108,204,79,67,98,24,153,26,67,228,97,80,67,232,153,26,67,84,76,82,67,104,97,26,67,28,145,85,67,98,248,200,25,67,
        172,97,94,67,232,37,25,67,12,46,98,67,88,114,21,67,220,254,114,67,98,24,21,18,67,190,35,129,67,152,157,17,67,202,137,130,67,152,157,17,67,126,250,132,67,98,152,157,17,67,110,189,139,67,152,123,24,67,66,93,144,67,120,169,37,67,70,122,146,67,98,72,187,
        39,67,62,207,146,67,88,199,40,67,234,239,146,67,152,110,43,67,98,46,147,67,98,72,222,49,67,226,197,147,67,248,84,54,67,78,227,147,67,168,35,61,67,22,163,147,67,99,109,88,184,49,67,50,39,146,67,98,184,196,43,67,150,217,145,67,8,171,36,67,202,231,144,67,
        216,208,32,67,138,231,143,67,98,184,216,26,67,118,90,142,67,88,146,22,67,234,109,139,67,56,252,20,67,2,208,135,67,98,152,146,20,67,58,223,134,67,248,146,20,67,142,28,132,67,56,252,20,67,130,153,130,67,98,216,192,21,67,244,149,127,67,72,133,22,67,132,
        167,123,67,24,208,24,67,188,150,113,67,98,216,66,27,67,116,214,102,67,232,24,28,67,172,123,98,67,168,180,28,67,44,81,93,67,98,184,228,28,67,132,185,91,67,248,32,29,67,196,24,90,67,136,58,29,67,12,179,89,67,98,248,88,29,67,28,58,89,67,8,97,29,67,220,204,
        87,67,216,81,29,67,220,147,85,67,98,232,68,29,67,44,181,83,67,200,75,29,67,196,175,81,67,200,96,29,67,20,22,81,67,98,120,138,29,67,148,228,79,67,200,237,29,67,4,181,78,67,104,61,30,67,100,115,78,67,98,88,89,30,67,108,92,78,67,184,170,31,67,252,24,78,
        67,40,43,33,67,148,221,77,67,98,40,157,41,67,116,143,76,67,152,178,47,67,52,215,76,67,104,122,52,67,60,193,78,67,98,56,253,54,67,172,194,79,67,24,159,57,67,244,223,81,67,56,188,58,67,244,199,83,67,98,136,222,59,67,212,184,85,67,168,7,60,67,52,120,86,
        67,136,27,60,67,172,42,90,67,98,72,47,60,67,108,220,93,67,136,3,60,67,44,77,96,67,40,102,59,67,92,68,100,67,98,24,179,58,67,92,199,104,67,24,251,57,67,132,214,107,67,168,217,54,67,20,161,119,67,98,184,53,54,67,140,10,122,67,8,126,53,67,132,232,124,67,
        136,65,53,67,36,0,126,67,98,248,4,53,67,196,23,127,67,152,136,52,67,106,163,128,67,24,45,52,67,70,109,129,67,98,72,244,50,67,82,31,132,67,24,188,50,67,38,157,133,67,24,96,51,67,210,221,134,67,98,168,202,51,67,30,174,135,67,200,52,52,67,166,22,136,67,
        8,17,53,67,34,136,136,67,98,184,246,54,67,118,130,137,67,56,42,58,67,62,124,137,67,88,51,61,67,90,120,136,67,98,168,246,62,67,106,225,135,67,40,229,63,67,182,80,135,67,200,85,65,67,46,246,133,67,98,104,35,70,67,82,114,129,67,56,223,73,67,188,116,118,
        67,248,151,76,67,20,107,102,67,98,72,53,78,67,196,231,92,67,152,138,78,67,212,208,89,67,8,156,78,67,236,184,83,67,98,200,170,78,67,100,151,78,67,200,138,78,67,60,10,79,67,72,27,80,67,252,84,78,67,98,168,110,81,67,92,187,77,67,120,46,83,67,20,114,77,67,
        104,63,87,67,204,41,77,67,98,168,75,94,67,140,172,76,67,152,93,98,67,180,49,77,67,168,66,102,67,252,20,79,67,98,104,228,105,67,172,215,80,67,216,35,108,67,92,136,83,67,72,230,108,67,28,7,87,67,98,8,69,109,67,92,187,88,67,184,81,109,67,188,242,93,67,40,
        252,108,67,12,12,96,67,98,40,134,108,67,124,241,98,67,216,59,107,67,244,40,105,67,248,74,106,67,228,4,109,67,98,24,191,105,67,108,66,111,67,104,190,104,67,180,121,115,67,120,16,104,67,84,99,118,67,98,216,22,101,67,122,146,129,67,88,136,98,67,14,219,133,
        67,184,104,96,67,18,5,136,67,98,232,131,92,67,10,253,139,67,136,7,86,67,142,14,143,67,8,144,78,67,118,118,144,67,98,216,252,75,67,146,242,144,67,232,135,68,67,42,192,145,67,40,243,63,67,90,9,146,67,98,200,100,61,67,54,50,146,67,184,15,52,67,194,69,146,
        67,184,183,49,67,50,39,146,67,99,109,8,159,63,67,50,75,145,67,98,104,163,75,67,98,149,144,67,24,168,82,67,94,67,143,67,168,248,87,67,102,182,140,67,98,248,92,92,67,218,154,138,67,136,154,95,67,34,227,135,67,136,10,98,67,106,68,132,67,98,152,64,99,67,
        2,120,130,67,168,101,105,67,108,39,107,67,104,184,106,67,212,29,100,67,98,120,191,107,67,12,167,94,67,40,219,107,67,100,171,93,67,120,196,107,67,52,154,90,67,98,104,179,107,67,100,77,88,67,104,161,107,67,84,193,87,67,248,64,107,67,228,153,86,67,98,24,
        232,105,67,76,121,82,67,120,171,102,67,204,16,80,67,120,226,96,67,220,226,78,67,98,184,114,95,67,228,151,78,67,104,139,94,67,188,135,78,67,248,171,91,67,180,133,78,67,98,8,144,86,67,44,130,78,67,56,228,82,67,100,185,78,67,56,11,82,67,180,22,79,67,98,
        200,158,80,67,116,179,79,67,56,1,80,67,4,104,81,67,56,1,80,67,212,188,84,67,98,56,1,80,67,20,41,87,67,232,172,79,67,220,31,91,67,40,20,79,67,164,225,95,67,98,40,172,77,67,20,24,107,67,40,58,75,67,100,228,117,67,232,172,71,67,182,73,128,67,98,168,243,
        69,67,90,225,130,67,200,89,69,67,74,166,131,67,248,231,67,67,226,28,133,67,98,136,240,64,67,242,29,136,67,24,221,61,67,70,154,137,67,72,212,57,67,182,250,137,67,98,152,94,53,67,78,101,138,67,40,217,49,67,234,162,136,67,200,153,49,67,54,222,133,67,98,
        248,110,49,67,30,255,131,67,232,182,50,67,38,193,128,67,56,62,54,67,140,60,116,67,98,104,106,56,67,188,16,108,67,184,49,57,67,244,165,104,67,200,230,57,67,28,47,100,67,98,200,176,58,67,76,52,95,67,72,189,58,67,228,179,94,67,56,187,58,67,76,169,91,67,
        98,248,184,58,67,188,87,88,67,248,142,58,67,252,76,87,67,216,179,57,67,4,36,85,67,98,136,148,56,67,252,78,82,67,136,243,52,67,188,246,79,67,104,56,48,67,164,3,79,67,98,40,149,46,67,132,175,78,67,8,98,42,67,252,91,78,67,136,136,41,67,164,126,78,67,98,
        168,42,41,67,156,141,78,67,136,96,39,67,212,169,78,67,136,142,37,67,100,189,78,67,98,232,17,33,67,156,237,78,67,120,1,32,67,76,78,79,67,216,67,31,67,228,248,80,67,98,184,237,30,67,212,186,81,67,120,225,30,67,60,42,82,67,184,202,30,67,52,74,85,67,98,152,
        146,30,67,124,255,92,67,248,163,29,67,148,204,98,67,216,14,26,67,60,191,114,67,98,168,89,22,67,98,160,129,67,152,124,21,67,226,214,132,67,104,82,22,67,78,70,135,67,98,232,35,23,67,82,169,137,67,248,114,25,67,66,237,139,67,168,170,28,67,10,128,141,67,
        98,72,180,32,67,126,121,143,67,88,6,40,67,218,219,144,67,24,36,49,67,38,95,145,67,98,104,210,51,67,198,133,145,67,88,139,60,67,190,121,145,67,24,159,63,67,46,75,145,67,99,109,180,113,182,67,214,154,147,67,98,204,139,185,67,190,90,147,67,204,110,187,67,
        102,20,147,67,76,178,188,67,190,177,146,67,98,12,107,190,67,74,43,146,67,180,176,191,67,14,110,145,67,212,181,192,67,150,92,144,67,98,44,167,193,67,202,95,143,67,84,6,194,67,114,156,142,67,204,92,194,67,30,248,140,67,98,228,199,194,67,142,239,138,67,
        124,195,194,67,82,152,136,67,84,81,194,67,2,231,134,67,98,172,22,194,67,58,8,134,67,108,124,193,67,146,185,132,67,220,47,193,67,238,114,132,67,108,132,254,192,67,94,69,132,67,108,84,203,191,67,162,119,132,67,98,100,136,188,67,70,0,133,67,180,64,187,67,
        246,37,133,67,92,2,185,67,106,63,133,67,98,100,232,182,67,66,87,133,67,44,60,181,67,82,66,133,67,116,233,180,67,26,12,133,67,98,108,203,180,67,106,248,132,67,140,159,180,67,218,201,132,67,4,136,180,67,162,164,132,67,98,132,99,180,67,234,106,132,67,244,
        94,180,67,10,74,132,67,44,105,180,67,90,197,131,67,98,116,120,180,67,38,254,130,67,44,174,180,67,218,131,130,67,60,17,181,67,154,70,130,67,98,180,104,181,67,142,16,130,67,124,197,181,67,46,11,130,67,132,46,184,67,134,24,130,67,98,76,111,185,67,126,31,
        130,67,36,191,185,67,74,26,130,67,156,127,186,67,234,241,129,67,98,188,151,187,67,50,183,129,67,196,222,188,67,202,74,129,67,244,167,189,67,234,229,128,67,98,204,136,190,67,46,117,128,67,28,119,191,67,12,254,126,67,84,249,191,67,244,1,125,67,98,4,146,
        192,67,60,174,122,67,100,190,192,67,212,18,121,67,140,187,192,67,116,249,117,67,98,148,185,192,67,228,200,115,67,36,177,192,67,148,43,115,67,92,130,192,67,156,198,113,67,98,164,69,192,67,124,247,111,67,52,207,191,67,12,241,109,67,36,128,191,67,132,92,
        109,67,98,4,79,191,67,60,0,109,67,116,70,191,67,28,254,108,67,124,189,190,67,244,44,109,67,98,116,216,188,67,212,210,109,67,116,42,185,67,188,132,110,67,108,108,184,67,92,94,110,67,98,60,8,184,67,44,74,110,67,36,245,183,67,44,54,110,67,140,200,183,67,
        52,179,109,67,98,100,156,183,67,108,49,109,67,84,151,183,67,44,251,108,67,236,159,183,67,196,0,108,67,98,92,166,183,67,172,67,107,67,68,187,183,67,124,158,106,67,116,221,183,67,148,25,106,67,108,36,17,184,67,132,80,105,67,108,156,153,184,67,28,51,105,
        67,98,172,228,184,67,228,34,105,67,220,6,186,67,204,246,104,67,124,30,187,67,4,209,104,67,98,180,98,192,67,204,26,104,67,188,253,194,67,180,198,102,67,212,204,196,67,68,225,99,67,98,172,77,198,67,36,121,97,67,204,96,199,67,84,163,93,67,212,168,199,67,
        252,172,89,67,98,156,198,199,67,100,9,88,67,124,198,199,67,156,112,83,67,212,168,199,67,4,251,81,67,98,36,110,199,67,180,29,79,67,212,253,198,67,124,143,76,67,52,121,198,67,68,18,75,67,98,204,45,198,67,108,57,74,67,20,28,198,67,28,30,74,67,28,216,197,
        67,204,25,74,67,98,68,30,197,67,244,13,74,67,164,106,196,67,204,40,74,67,180,185,195,67,228,106,74,67,98,60,188,194,67,140,201,74,67,60,73,189,67,148,199,74,67,212,173,184,67,236,102,74,67,98,228,105,179,67,100,248,73,67,52,209,177,67,148,235,73,67,148,
        126,175,67,12,29,74,67,98,12,227,171,67,236,105,74,67,84,54,168,67,52,38,75,67,124,32,167,67,220,201,75,67,108,244,126,166,67,244,40,76,67,108,108,113,166,67,204,42,77,67,98,244,105,166,67,156,184,77,67,12,106,166,67,244,169,79,67,108,113,166,67,244,
        123,81,67,98,220,133,166,67,148,102,86,67,164,68,166,67,124,176,93,67,132,217,165,67,12,118,98,67,98,148,93,165,67,20,251,103,67,148,93,164,67,156,209,112,67,124,216,162,67,4,2,125,67,98,52,29,161,67,166,114,133,67,60,177,160,67,186,254,135,67,124,202,
        160,67,102,234,138,67,98,132,225,160,67,130,149,141,67,252,85,161,67,202,4,143,67,100,124,162,67,198,66,144,67,98,228,178,163,67,38,146,145,67,4,122,165,67,22,104,146,67,28,67,168,67,222,249,146,67,98,252,25,170,67,30,90,147,67,244,155,172,67,186,163,
        147,67,4,31,175,67,50,195,147,67,98,124,101,176,67,42,211,147,67,28,237,180,67,62,186,147,67,172,113,182,67,218,154,147,67,99,109,196,161,173,67,22,47,146,67,98,60,137,171,67,34,15,146,67,252,200,169,67,234,205,145,67,236,10,168,67,234,94,145,67,98,132,
        239,166,67,98,24,145,67,76,128,165,67,174,153,144,67,68,219,164,67,126,69,144,67,98,4,182,163,67,230,175,143,67,228,203,162,67,66,103,142,67,132,116,162,67,194,230,140,67,98,12,86,162,67,146,96,140,67,12,78,162,67,226,226,139,67,44,78,162,67,14,139,138,
        67,98,44,78,162,67,82,55,135,67,204,158,162,67,14,135,133,67,236,6,165,67,116,203,119,67,98,156,99,166,67,12,229,108,67,252,220,166,67,236,128,104,67,52,87,167,67,36,93,98,67,98,20,209,167,67,172,61,92,67,108,231,167,67,172,32,90,67,124,238,167,67,140,
        12,84,67,108,244,244,167,67,76,129,78,67,108,68,46,169,67,4,39,78,67,98,28,39,173,67,252,1,77,67,20,61,178,67,132,199,76,67,124,76,185,67,204,109,77,67,98,108,88,186,67,124,134,77,67,76,167,188,67,92,172,77,67,132,109,190,67,12,194,77,67,98,196,126,193,
        67,140,231,77,67,100,180,193,67,52,229,77,67,108,176,194,67,124,146,77,67,98,132,237,195,67,92,42,77,67,156,185,196,67,20,55,77,67,36,58,197,67,228,186,77,67,98,116,139,197,67,60,14,78,67,100,150,197,67,172,46,78,67,212,199,197,67,164,94,79,67,98,12,
        190,198,67,52,72,85,67,236,90,198,67,148,184,91,67,228,204,196,67,100,172,95,67,98,44,169,195,67,204,145,98,67,100,114,193,67,84,120,100,67,4,78,190,67,228,63,101,67,98,220,251,188,67,196,147,101,67,100,34,185,67,124,24,102,67,52,202,184,67,12,254,101,
        67,98,36,71,184,67,188,214,101,67,244,158,183,67,12,72,102,67,148,41,183,67,172,22,103,67,98,132,121,182,67,156,76,104,67,164,6,182,67,108,207,106,67,60,10,182,67,116,94,109,67,98,156,12,182,67,220,19,111,67,20,26,182,67,236,87,111,67,92,146,182,67,116,
        15,112,67,98,100,147,183,67,164,151,113,67,44,11,185,67,108,207,113,67,244,242,187,67,188,219,112,67,98,140,83,189,67,52,104,112,67,108,169,189,67,124,88,112,67,84,239,189,67,180,126,112,67,98,180,126,190,67,36,205,112,67,36,196,190,67,76,63,113,67,244,
        4,191,67,52,71,114,67,98,236,63,191,67,116,55,115,67,148,65,191,67,76,80,115,67,156,63,191,67,228,185,117,67,98,212,61,191,67,204,224,119,67,60,54,191,67,36,103,120,67,220,6,191,67,148,171,121,67,98,156,158,190,67,220,116,124,67,76,220,189,67,76,59,126,
        67,84,135,188,67,116,131,127,67,98,244,22,187,67,250,114,128,67,124,238,185,67,102,160,128,67,156,251,182,67,42,155,128,67,98,124,94,181,67,66,152,128,67,140,238,180,67,198,157,128,67,92,167,180,67,94,184,128,67,98,212,223,179,67,226,2,129,67,140,204,
        179,67,14,21,129,67,116,107,179,67,10,226,129,67,98,92,118,178,67,98,231,131,67,204,133,178,67,22,109,133,67,164,150,179,67,206,37,134,67,98,76,58,180,67,162,148,134,67,68,145,182,67,150,222,134,67,100,205,184,67,162,202,134,67,98,236,211,186,67,138,
        184,134,67,28,66,190,67,102,77,134,67,228,236,190,67,106,11,134,67,98,220,88,191,67,178,225,133,67,60,189,191,67,110,224,133,67,100,9,192,67,222,7,134,67,98,140,211,192,67,106,112,134,67,44,36,193,67,202,143,135,67,204,38,193,67,102,1,138,67,98,220,40,
        193,67,110,252,139,67,44,248,192,67,110,249,140,67,108,83,192,67,46,75,142,67,98,228,195,191,67,94,113,143,67,244,224,190,67,174,61,144,67,36,152,189,67,190,192,144,67,98,164,136,187,67,254,146,145,67,76,126,185,67,74,240,145,67,164,189,181,67,210,39,
        146,67,98,172,125,180,67,82,58,146,67,76,181,174,67,134,63,146,67,196,161,173,67,26,47,146,67,99,109,164,235,179,67,98,126,145,67,98,76,71,185,67,226,69,145,67,252,17,188,67,226,205,144,67,252,191,189,67,214,215,143,67,98,92,186,190,67,158,72,143,67,
        244,91,191,67,202,170,142,67,36,177,191,67,98,242,141,67,98,172,130,192,67,230,44,140,67,228,172,192,67,58,186,137,67,84,33,192,67,130,112,135,67,98,92,12,192,67,90,24,135,67,236,245,191,67,2,203,134,67,140,239,191,67,158,196,134,67,98,52,233,191,67,
        70,190,134,67,164,105,191,67,134,204,134,67,44,212,190,67,62,228,134,67,98,172,173,187,67,126,100,135,67,252,153,184,67,146,155,135,67,244,81,182,67,134,124,135,67,98,68,81,180,67,74,97,135,67,228,159,179,67,86,41,135,67,124,185,178,67,46,90,134,67,98,
        228,247,177,67,34,172,133,67,76,220,177,67,226,55,133,67,116,27,178,67,130,192,131,67,98,44,94,178,67,234,51,130,67,76,225,178,67,122,2,129,67,132,135,179,67,146,112,128,67,98,108,39,180,67,76,200,127,67,204,65,180,67,12,191,127,67,124,198,182,67,28,
        189,127,67,98,100,92,185,67,4,187,127,67,20,106,186,67,204,132,127,67,236,127,187,67,156,201,126,67,98,60,94,188,67,196,51,126,67,52,68,189,67,244,9,125,67,228,160,189,67,196,7,124,67,98,196,118,190,67,36,180,121,67,164,226,190,67,132,174,117,67,252,
        122,190,67,28,230,115,67,98,228,79,190,67,84,40,115,67,52,231,189,67,44,99,114,67,148,152,189,67,180,59,114,67,98,116,124,189,67,140,45,114,67,4,150,188,67,140,76,114,67,132,152,187,67,140,128,114,67,98,204,229,184,67,68,14,115,67,4,124,183,67,228,213,
        114,67,52,26,182,67,116,165,113,67,98,60,167,181,67,148,66,113,67,204,133,181,67,228,199,112,67,124,92,181,67,76,237,110,67,98,84,23,181,67,4,211,107,67,244,158,181,67,164,53,104,67,84,175,182,67,204,229,101,67,108,204,37,183,67,100,228,100,67,108,4,
        216,184,67,4,172,100,67,98,68,158,188,67,156,46,100,67,228,36,191,67,100,161,99,67,108,127,192,67,36,0,99,67,98,172,228,193,67,228,89,98,67,108,6,195,67,220,35,97,67,236,240,195,67,220,80,95,67,98,212,158,196,67,132,246,93,67,252,18,197,67,156,109,92,
        67,204,61,197,67,180,234,90,67,98,20,101,197,67,180,135,89,67,44,150,197,67,164,20,86,67,44,150,197,67,220,180,84,67,98,44,150,197,67,140,125,82,67,92,72,197,67,76,98,80,67,244,199,196,67,76,31,79,67,98,180,161,196,67,28,191,78,67,156,161,196,67,20,191,
        78,67,84,153,195,67,196,243,78,67,98,60,30,194,67,92,63,79,67,44,252,187,67,84,51,79,67,172,173,184,67,100,222,78,67,98,12,51,183,67,100,184,78,67,252,33,181,67,4,141,78,67,4,22,180,67,20,126,78,67,98,36,28,177,67,124,83,78,67,108,68,172,67,36,202,78,
        67,196,159,169,67,116,126,79,67,98,132,26,169,67,252,161,79,67,148,170,168,67,12,191,79,67,252,166,168,67,12,191,79,67,98,116,163,168,67,12,191,79,67,252,166,168,67,212,244,80,67,220,173,168,67,116,111,82,67,98,140,195,168,67,132,208,86,67,124,126,168,
        67,228,154,93,67,60,255,167,67,220,154,99,67,98,12,136,167,67,244,57,105,67,172,135,166,67,12,49,114,67,164,71,165,67,140,238,123,67,98,84,185,163,67,226,6,132,67,220,92,163,67,174,221,133,67,148,13,163,67,74,77,137,67,98,124,222,162,67,162,87,139,67,
        76,4,163,67,190,158,140,67,68,147,163,67,194,207,141,67,98,20,44,164,67,250,21,143,67,20,228,164,67,174,171,143,67,132,121,166,67,150,43,144,67,98,68,35,170,67,118,83,145,67,116,57,174,67,110,186,145,67,180,235,179,67,94,126,145,67,99,109,156,226,143,
        67,178,133,147,67,98,68,19,148,67,98,54,147,67,132,80,151,67,66,88,146,67,236,82,154,67,222,185,144,67,98,4,96,155,67,38,41,144,67,188,17,156,67,150,167,143,67,44,200,156,67,42,239,142,67,98,116,139,158,67,2,39,141,67,108,134,159,67,90,239,138,67,236,
        133,159,67,250,187,136,67,98,236,133,159,67,106,106,135,67,68,90,159,67,38,181,134,67,212,173,158,67,194,52,133,67,98,92,56,158,67,234,46,132,67,252,39,158,67,122,24,132,67,124,150,157,67,18,182,131,67,108,204,46,157,67,234,111,131,67,108,228,185,155,
        67,62,44,132,67,98,124,21,154,67,138,0,133,67,156,97,153,67,18,75,133,67,196,12,152,67,14,178,133,67,98,236,52,150,67,170,64,134,67,244,126,148,67,46,132,134,67,84,189,146,67,182,131,134,67,98,252,59,143,67,170,130,134,67,100,21,141,67,158,95,133,67,
        156,211,139,67,2,220,130,67,98,76,24,139,67,78,101,129,67,20,196,138,67,180,173,127,67,52,178,138,67,92,5,123,67,98,100,151,138,67,196,10,116,67,196,95,139,67,188,8,108,67,92,200,140,67,116,172,101,67,98,204,181,141,67,84,124,97,67,36,99,142,67,172,255,
        95,67,188,129,143,67,36,177,95,67,98,236,6,145,67,124,70,95,67,76,159,145,67,68,225,97,67,204,31,145,67,188,196,102,67,98,68,216,144,67,172,130,105,67,196,30,144,67,28,155,109,67,180,109,143,67,100,90,112,67,98,108,1,143,67,100,8,114,67,116,6,143,67,
        164,54,114,67,92,180,143,67,212,229,114,67,98,188,243,146,67,44,43,118,67,196,52,152,67,116,129,118,67,84,65,156,67,4,180,115,67,98,84,229,159,67,244,46,113,67,204,120,162,67,28,126,107,67,132,52,163,67,140,89,100,67,98,164,169,163,67,116,228,95,67,76,
        87,163,67,60,167,91,67,196,70,162,67,244,28,88,67,98,132,43,161,67,4,111,84,67,140,54,159,67,212,217,80,67,68,55,157,67,4,217,78,67,98,244,84,154,67,116,244,75,67,204,122,150,67,84,85,74,67,4,94,146,67,220,71,74,67,98,4,32,145,67,180,67,74,67,36,220,
        141,67,196,139,74,67,100,191,141,67,180,173,74,67,98,148,183,141,67,212,182,74,67,204,62,141,67,4,222,74,67,252,178,140,67,148,4,75,67,98,220,2,137,67,28,9,76,67,204,36,133,67,148,197,78,67,12,39,130,67,220,122,82,67,98,232,110,122,67,76,153,88,67,136,
        81,115,67,164,23,99,67,72,246,111,67,220,123,112,67,98,104,153,110,67,196,235,117,67,120,58,110,67,100,101,121,67,168,60,110,67,2,83,128,67,98,184,62,110,67,66,207,130,67,40,69,110,67,162,12,131,67,152,174,110,67,94,59,132,67,98,8,85,111,67,2,25,134,
        67,8,223,111,67,50,44,135,67,200,164,112,67,198,36,136,67,98,56,141,114,67,158,138,138,67,184,77,117,67,142,168,140,67,184,175,120,67,158,84,142,67,98,248,52,126,67,46,15,145,67,124,255,130,67,106,190,146,67,244,37,136,67,202,111,147,67,98,84,101,138,
        67,50,189,147,67,4,159,140,67,130,195,147,67,140,226,143,67,186,133,147,67,99,109,220,180,139,67,242,46,146,67,98,44,202,136,67,186,24,146,67,28,180,133,67,14,154,145,67,68,67,131,67,94,212,144,67,98,40,240,127,67,154,201,143,67,24,193,121,67,170,80,
        141,67,56,249,117,67,82,54,138,67,98,200,32,115,67,110,224,135,67,104,212,113,67,210,147,133,67,8,71,113,67,118,229,129,67,98,248,94,112,67,220,180,119,67,72,177,115,67,236,16,106,67,168,194,121,67,12,245,96,67,98,232,230,123,67,244,189,93,67,248,146,
        127,67,164,184,89,67,4,53,129,67,92,123,87,67,98,204,123,130,67,4,120,85,67,28,137,132,67,132,18,83,67,52,52,134,67,108,165,81,67,98,132,86,139,67,244,65,77,67,204,66,146,67,116,15,76,67,20,51,152,67,196,136,78,67,98,180,79,155,67,164,212,79,67,20,162,
        157,67,92,58,82,67,156,156,159,67,52,48,86,67,98,124,131,160,67,76,254,87,67,196,179,160,67,196,119,88,67,4,9,161,67,12,197,89,67,98,20,63,161,67,84,152,90,67,52,127,161,67,36,214,91,67,124,151,161,67,76,135,92,67,98,180,239,161,67,100,10,95,67,36,240,
        161,67,60,97,98,67,156,152,161,67,252,2,101,67,98,92,97,161,67,36,172,102,67,52,188,160,67,100,86,105,67,236,59,160,67,108,163,106,67,98,220,124,159,67,92,147,108,67,124,19,158,67,20,193,110,67,132,192,156,67,12,3,112,67,98,132,63,154,67,244,99,114,67,
        132,12,151,67,20,52,115,67,44,251,147,67,156,61,114,67,98,172,134,147,67,12,25,114,67,60,213,146,67,180,227,113,67,228,112,146,67,4,199,113,67,98,132,4,145,67,244,94,113,67,44,245,144,67,108,221,112,67,68,213,145,67,28,164,108,67,98,244,90,146,67,228,
        30,106,67,228,181,146,67,36,1,103,67,100,194,146,67,228,131,100,67,98,212,202,146,67,76,214,98,67,108,197,146,67,140,32,98,67,4,170,146,67,204,73,97,67,98,244,102,146,67,44,60,95,67,20,200,145,67,116,177,93,67,244,224,144,67,4,218,92,67,98,180,131,144,
        67,20,131,92,67,20,77,144,67,244,109,92,67,156,205,143,67,108,111,92,67,98,44,18,143,67,132,113,92,67,92,147,142,67,52,200,92,67,236,216,141,67,108,197,93,67,98,196,226,140,67,196,19,95,67,204,63,140,67,28,201,96,67,36,135,139,67,188,254,99,67,98,132,
        177,137,67,148,40,108,67,92,227,136,67,60,126,117,67,196,69,137,67,60,35,126,67,98,52,132,137,67,94,207,129,67,4,104,138,67,34,249,131,67,36,242,139,67,26,145,133,67,98,44,60,141,67,178,230,134,67,44,72,142,67,246,108,135,67,148,117,144,67,250,211,135,
        67,98,236,181,145,67,46,15,136,67,44,8,148,67,42,19,136,67,148,98,149,67,102,220,135,67,98,84,55,151,67,90,146,135,67,188,92,153,67,82,235,134,67,228,73,155,67,238,16,134,67,98,212,151,156,67,14,125,133,67,244,230,156,67,142,115,133,67,44,71,157,67,198,
        211,133,67,98,116,247,157,67,14,132,134,67,220,52,158,67,222,124,136,67,140,208,157,67,194,60,138,67,98,116,19,157,67,202,136,141,67,52,236,153,67,194,254,143,67,156,226,148,67,162,52,145,67,98,52,254,145,67,142,230,145,67,212,139,142,67,134,68,146,67,
        236,180,139,67,230,46,146,67,99,109,156,173,143,67,154,83,145,67,98,148,112,147,67,106,3,145,67,212,113,150,67,174,54,144,67,212,83,153,67,38,194,142,67,98,20,186,154,67,74,13,142,67,28,247,155,67,234,194,140,67,44,170,156,67,206,71,139,67,98,20,28,157,
        67,162,86,138,67,84,62,157,67,234,186,137,67,204,62,157,67,218,163,136,67,98,204,62,157,67,30,176,135,67,180,51,157,67,130,116,135,67,4,220,156,67,90,164,134,67,108,140,178,156,67,230,65,134,67,108,28,219,155,67,186,166,134,67,98,124,230,152,67,222,8,
        136,67,4,235,148,67,118,232,136,67,28,35,146,67,110,200,136,67,98,60,76,143,67,178,167,136,67,204,239,140,67,62,180,135,67,252,83,139,67,74,10,134,67,98,172,174,138,67,82,95,133,67,108,112,138,67,118,4,133,67,28,234,137,67,246,249,131,67,98,148,209,136,
        67,138,205,129,67,36,96,136,67,4,247,126,67,100,119,136,67,172,201,120,67,98,4,137,136,67,12,28,116,67,212,186,136,67,100,103,113,67,140,99,137,67,84,246,107,67,98,4,212,137,67,220,85,104,67,156,69,138,67,244,175,101,67,212,210,138,67,60,102,99,67,98,
        68,161,139,67,84,14,96,67,116,221,140,67,196,243,92,67,28,201,141,67,236,244,91,67,98,180,252,142,67,52,168,90,67,68,147,144,67,124,177,90,67,236,186,145,67,12,12,92,67,98,212,235,146,67,100,113,93,67,164,155,147,67,108,229,96,67,28,127,147,67,172,237,
        100,67,98,92,108,147,67,4,147,103,67,36,34,147,67,236,232,105,67,84,100,146,67,196,210,109,67,98,68,46,146,67,20,240,110,67,4,7,146,67,116,227,111,67,28,13,146,67,148,239,111,67,98,20,39,146,67,148,35,112,67,116,35,148,67,188,217,112,67,228,23,149,67,
        156,6,113,67,98,60,154,151,67,156,124,113,67,92,252,153,67,44,192,112,67,84,82,156,67,28,203,110,67,98,220,102,157,67,108,227,109,67,244,0,158,67,36,31,109,67,244,185,158,67,44,187,107,67,98,76,15,160,67,60,42,105,67,76,226,160,67,188,212,101,67,76,25,
        161,67,180,34,98,67,98,188,88,161,67,76,223,93,67,4,180,160,67,76,94,90,67,140,18,159,67,228,25,87,67,98,84,179,156,67,84,89,82,67,140,88,153,67,124,195,79,67,60,122,148,67,116,239,78,67,98,4,11,145,67,220,89,78,67,164,252,140,67,148,10,79,67,68,216,
        137,67,164,190,80,67,98,172,133,131,67,44,44,84,67,152,85,126,67,244,246,90,67,168,185,120,67,172,197,101,67,98,24,120,117,67,164,11,108,67,88,181,115,67,100,23,114,67,232,229,114,67,220,199,121,67,98,24,160,114,67,52,94,124,67,184,158,114,67,146,45,
        130,67,168,227,114,67,202,45,131,67,98,184,182,115,67,218,61,134,67,248,56,117,67,58,89,136,67,120,36,120,67,158,131,138,67,98,88,208,121,67,242,192,139,67,168,253,122,67,182,112,140,67,232,196,124,67,114,54,141,67,98,12,233,128,67,30,104,143,67,172,
        135,132,67,126,206,144,67,164,228,136,67,210,70,145,67,98,52,46,138,67,86,106,145,67,148,64,142,67,254,113,145,67,156,173,143,67,154,83,145,67,99,109,172,241,204,67,66,151,144,67,98,28,225,204,67,238,248,143,67,20,222,204,67,166,22,143,67,140,232,204,
        67,138,233,141,67,98,188,3,205,67,174,215,138,67,52,4,205,67,22,231,138,67,100,207,204,67,82,178,138,67,98,180,176,204,67,166,147,138,67,236,136,204,67,134,132,138,67,228,90,204,67,14,134,138,67,98,124,52,204,67,74,135,138,67,132,166,203,67,90,139,138,
        67,116,31,203,67,254,142,138,67,108,236,41,202,67,166,149,138,67,108,132,227,200,67,70,57,139,67,98,188,1,200,67,126,170,139,67,84,123,199,67,34,225,139,67,100,47,199,67,150,234,139,67,108,172,193,198,67,58,248,139,67,108,36,169,198,67,66,143,140,67,
        98,84,146,198,67,138,27,141,67,132,60,198,67,218,168,142,67,156,30,198,67,110,16,143,67,98,68,18,198,67,38,59,143,67,148,67,198,67,110,78,143,67,196,135,201,67,190,101,144,67,98,44,111,203,67,142,8,145,67,204,0,205,67,122,139,145,67,60,4,205,67,174,136,
        145,67,98,172,7,205,67,222,133,145,67,84,255,204,67,62,25,145,67,188,241,204,67,66,151,144,67,99,109,188,34,227,67,202,82,145,67,98,132,207,227,67,162,250,144,67,180,248,227,67,134,254,143,67,100,147,227,67,170,160,142,67,98,212,63,227,67,250,127,141,
        67,204,23,227,67,194,68,141,67,4,140,226,67,246,26,141,67,98,20,86,225,67,74,190,140,67,156,106,224,67,98,7,140,67,116,49,224,67,226,70,139,67,98,36,14,224,67,254,207,138,67,76,33,224,67,154,43,138,67,20,93,224,67,94,208,137,67,98,212,151,224,67,178,
        118,137,67,196,138,224,67,10,29,137,67,52,47,224,67,174,149,136,67,98,60,1,224,67,194,81,136,67,212,199,223,67,6,244,135,67,172,175,223,67,110,197,135,67,98,52,75,223,67,158,3,135,67,228,157,222,67,70,226,134,67,124,92,221,67,234,82,135,67,98,164,245,
        220,67,250,118,135,67,44,73,220,67,42,175,135,67,60,221,219,67,198,207,135,67,98,12,2,219,67,14,18,136,67,132,182,218,67,90,59,136,67,68,155,218,67,230,127,136,67,98,28,145,218,67,126,153,136,67,108,45,218,67,234,132,137,67,204,189,217,67,14,139,138,
        67,98,44,78,217,67,50,145,139,67,212,242,216,67,238,114,140,67,220,242,216,67,182,128,140,67,98,220,242,216,67,62,151,140,67,28,156,218,67,130,203,141,67,12,68,220,67,82,232,142,67,98,204,69,223,67,118,237,144,67,204,4,226,67,170,228,145,67,196,34,227,
        67,198,82,145,67,99,109,116,176,215,67,54,47,139,67,98,100,188,215,67,166,32,139,67,4,209,215,67,30,236,138,67,68,222,215,67,118,186,138,67,98,140,235,215,67,210,136,138,67,36,28,216,67,42,18,138,67,68,74,216,67,202,178,137,67,98,84,168,216,67,74,240,
        136,67,12,175,216,67,54,206,136,67,116,119,216,67,54,206,136,67,98,108,38,216,67,54,206,136,67,36,84,214,67,194,62,137,67,204,10,214,67,6,100,137,67,98,108,118,213,67,98,175,137,67,116,141,213,67,214,226,137,67,100,160,214,67,158,178,138,67,98,28,111,
        215,67,214,78,139,67,196,139,215,67,246,91,139,67,116,176,215,67,54,47,139,67,99,109,48,133,180,66,198,223,138,67,98,112,117,181,66,146,49,138,67,16,138,181,66,206,63,138,67,144,114,179,66,70,34,138,67,98,176,189,172,66,170,195,137,67,208,188,165,66,
        10,197,136,67,144,121,161,66,218,148,135,67,98,176,218,157,66,134,146,134,67,240,170,154,66,26,28,133,67,80,105,153,66,110,225,131,67,98,112,141,151,66,226,15,130,67,144,113,152,66,164,152,127,67,240,174,155,66,164,54,124,67,98,48,33,159,66,132,157,120,
        67,176,90,165,66,252,136,117,67,176,91,172,66,76,246,115,67,98,16,163,180,66,76,26,114,67,144,88,190,66,132,240,113,67,48,200,198,66,132,132,115,67,98,80,198,199,66,20,180,115,67,240,159,200,66,84,212,115,67,176,171,200,66,60,204,115,67,98,80,183,200,
        66,36,196,115,67,240,238,200,66,12,86,115,67,240,38,201,66,148,215,114,67,98,112,159,201,66,132,199,113,67,112,185,201,66,76,213,113,67,208,46,198,66,52,68,113,67,98,176,85,180,66,252,104,110,67,240,227,160,66,108,132,114,67,176,100,152,66,100,240,122,
        67,98,176,17,145,66,82,25,129,67,16,174,147,66,158,109,133,67,16,248,158,66,142,95,136,67,98,208,32,164,66,30,184,137,67,240,207,169,66,98,143,138,67,208,10,177,66,14,12,139,67,98,208,57,178,66,118,32,139,67,16,103,179,66,134,47,139,67,48,168,179,66,
        134,45,139,67,98,144,233,179,66,142,43,139,67,240,76,180,66,142,8,139,67,48,133,180,66,198,223,138,67,99,109,196,222,203,67,62,47,137,67,98,36,239,204,67,6,22,137,67,220,20,205,67,190,13,137,67,252,54,205,67,158,227,136,67,98,12,76,205,67,154,201,136,
        67,156,93,205,67,106,150,136,67,12,94,205,67,214,113,136,67,98,12,94,205,67,66,77,136,67,180,135,205,67,82,115,135,67,188,185,205,67,138,141,134,67,108,172,20,206,67,198,235,132,67,108,60,10,205,67,18,166,131,67,98,172,119,204,67,238,242,130,67,220,197,
        203,67,94,31,130,67,28,127,203,67,242,207,129,67,108,116,254,202,67,130,63,129,67,108,76,66,202,67,178,110,129,67,98,60,60,201,67,106,176,129,67,108,40,200,67,194,26,130,67,172,173,198,67,18,208,130,67,108,140,105,197,67,62,107,131,67,108,140,139,197,
        67,242,172,131,67,98,4,240,197,67,54,111,132,67,132,111,198,67,254,12,134,67,60,181,198,67,234,114,135,67,98,12,189,198,67,230,154,135,67,236,200,198,67,110,158,135,67,156,40,199,67,62,149,135,67,98,92,223,199,67,174,131,135,67,124,116,200,67,214,213,
        135,67,76,235,200,67,130,141,136,67,98,76,52,201,67,90,254,136,67,164,200,201,67,230,70,137,67,76,109,202,67,62,74,137,67,98,60,144,202,67,74,75,137,67,132,54,203,67,206,62,137,67,196,222,203,67,58,47,137,67,99,109,44,183,208,67,94,162,136,67,98,4,32,
        209,67,202,135,136,67,20,122,209,67,162,110,136,67,76,127,209,67,130,106,136,67,98,140,142,209,67,94,94,136,67,180,7,208,67,170,221,134,67,172,194,207,67,234,180,134,67,108,228,131,207,67,206,143,134,67,108,244,108,207,67,74,203,134,67,98,180,56,207,
        67,158,82,135,67,100,21,207,67,150,15,136,67,228,34,207,67,130,87,136,67,98,124,51,207,67,238,175,136,67,44,121,207,67,122,233,136,67,148,195,207,67,42,220,136,67,98,180,224,207,67,246,214,136,67,84,78,208,67,238,188,136,67,44,183,208,67,90,162,136,67,
        99,109,176,232,183,66,18,49,136,67,98,80,81,184,66,218,218,135,67,80,190,184,66,42,133,135,67,176,218,184,66,158,114,135,67,98,144,2,185,66,162,88,135,67,144,214,184,66,238,80,135,67,144,26,184,66,238,80,135,67,98,16,126,182,66,238,80,135,67,144,84,178,
        66,74,253,134,67,144,227,175,66,214,171,134,67,98,16,87,171,66,22,20,134,67,208,210,166,66,46,181,132,67,176,70,165,66,66,115,131,67,98,16,225,163,66,158,80,130,67,240,8,164,66,246,227,128,67,240,172,165,66,20,154,127,67,98,240,32,168,66,228,87,124,67,
        48,11,174,66,84,174,121,67,144,3,181,66,172,178,120,67,98,176,134,185,66,196,15,120,67,144,200,191,66,36,90,120,67,80,250,195,66,140,100,121,67,108,176,137,197,66,156,199,121,67,108,144,231,197,66,180,19,121,67,98,48,27,198,66,188,176,120,67,240,123,
        198,66,236,1,120,67,80,190,198,66,76,143,119,67,98,208,0,199,66,164,28,119,67,176,43,199,66,28,185,118,67,144,29,199,66,20,178,118,67,98,112,15,199,66,4,171,118,67,240,11,198,66,164,118,118,67,176,220,196,66,148,61,118,67,98,176,193,188,66,20,183,116,
        67,48,162,179,66,252,21,117,67,144,58,172,66,220,61,119,67,98,112,178,168,66,20,69,120,67,80,23,164,66,164,146,122,67,240,8,162,66,188,86,124,67,98,16,121,155,66,154,252,128,67,48,109,157,66,234,152,132,67,16,186,166,66,142,215,134,67,98,80,91,170,66,
        214,183,135,67,112,71,175,66,14,106,136,67,240,194,179,66,134,175,136,67,98,176,13,183,66,134,226,136,67,176,17,183,66,10,226,136,67,176,232,183,66,18,49,136,67,99,109,132,163,213,67,142,176,135,67,98,36,53,214,67,98,149,135,67,60,234,214,67,138,121,
        135,67,252,53,215,67,166,114,135,67,98,180,78,216,67,30,89,135,67,188,89,217,67,18,144,134,67,108,224,217,67,218,112,133,67,98,236,36,218,67,186,222,132,67,212,109,218,67,94,171,131,67,188,126,218,67,102,213,130,67,98,20,146,218,67,66,224,129,67,228,
        47,218,67,250,69,129,67,20,172,216,67,36,240,127,67,98,92,54,216,67,116,37,127,67,164,175,215,67,108,44,126,67,188,128,215,67,180,198,125,67,98,116,220,214,67,196,98,124,67,44,95,214,67,20,75,124,67,172,106,212,67,76,49,125,67,98,100,179,211,67,148,133,
        125,67,188,214,210,67,220,227,125,67,76,128,210,67,204,2,126,67,98,252,34,210,67,60,36,126,67,60,212,209,67,28,90,126,67,108,190,209,67,132,135,126,67,98,44,170,209,67,140,177,126,67,76,60,209,67,174,138,128,67,44,202,208,67,146,235,129,67,108,180,250,
        207,67,42,109,132,67,108,188,44,209,67,190,161,133,67,98,20,213,209,67,118,75,134,67,20,171,210,67,138,24,135,67,68,8,211,67,118,105,135,67,108,188,177,211,67,150,252,135,67,108,60,38,212,67,66,239,135,67,98,84,102,212,67,226,231,135,67,228,17,213,67,
        182,203,135,67,132,163,213,67,138,176,135,67,99,109,124,114,212,67,250,10,133,67,98,252,216,211,67,222,203,132,67,36,114,211,67,14,109,132,67,60,48,211,67,218,225,131,67,98,156,217,210,67,234,42,131,67,228,252,210,67,114,105,130,67,76,148,211,67,118,
        189,129,67,98,68,124,212,67,234,181,128,67,204,248,213,67,182,165,128,67,164,253,214,67,74,152,129,67,98,36,211,215,67,210,94,130,67,156,203,215,67,254,190,131,67,4,237,214,67,34,163,132,67,98,12,73,214,67,54,75,133,67,52,104,213,67,254,111,133,67,124,
        114,212,67,246,10,133,67,99,109,16,104,187,66,58,107,133,67,98,208,19,188,66,66,224,132,67,48,160,188,66,126,107,132,67,16,160,188,66,190,103,132,67,98,240,159,188,66,2,100,132,67,240,157,187,66,66,96,132,67,144,98,186,66,110,95,132,67,98,48,69,177,66,
        78,89,132,67,144,187,172,66,242,135,129,67,80,52,179,66,148,211,127,67,98,144,192,182,66,124,13,126,67,144,98,188,66,156,205,125,67,240,157,192,66,132,59,127,67,108,80,22,194,66,172,186,127,67,108,48,160,194,66,100,221,126,67,98,240,235,194,66,180,99,
        126,67,80,120,195,66,132,117,125,67,208,215,195,66,36,204,124,67,108,176,133,196,66,44,152,123,67,108,112,229,194,66,188,46,123,67,98,112,78,191,66,212,69,122,67,48,254,185,66,140,2,122,67,144,227,181,66,4,138,122,67,98,176,250,176,66,4,44,123,67,144,
        54,172,66,188,30,125,67,144,250,169,66,60,104,127,67,98,208,91,167,66,106,11,129,67,240,111,167,66,14,165,130,67,48,48,170,66,6,247,131,67,98,144,218,172,66,106,62,133,67,16,190,179,66,78,96,134,67,144,1,185,66,134,102,134,67,108,144,47,186,66,222,103,
        134,67,108,240,103,187,66,46,107,133,67,99,109,12,60,220,67,246,22,134,67,98,204,115,220,67,186,1,134,67,44,177,220,67,242,243,133,67,108,196,220,67,86,248,133,67,98,180,215,220,67,178,252,133,67,188,9,221,67,254,241,133,67,164,51,221,67,118,224,133,
        67,98,52,118,221,67,170,196,133,67,220,127,221,67,98,183,133,67,220,127,221,67,202,119,133,67,98,220,127,221,67,22,18,133,67,164,13,221,67,98,110,132,67,116,154,220,67,6,47,132,67,98,100,103,220,67,238,18,132,67,4,55,220,67,186,4,132,67,84,45,220,67,
        246,14,132,67,98,100,20,220,67,102,41,132,67,84,110,219,67,90,218,133,67,84,110,219,67,242,0,134,67,98,84,110,219,67,66,73,134,67,196,167,219,67,106,79,134,67,12,60,220,67,242,22,134,67,99,109,180,130,224,67,70,172,132,67,98,148,178,224,67,166,149,132,
        67,36,157,225,67,238,48,132,67,252,139,226,67,118,204,131,67,98,100,93,229,67,14,157,130,67,20,184,230,67,106,208,129,67,180,240,231,67,146,157,128,67,98,36,129,232,67,198,15,128,67,156,173,232,67,140,171,127,67,12,192,232,67,28,30,127,67,98,140,252,
        232,67,188,77,125,67,4,85,232,67,188,227,123,67,172,211,230,67,60,246,122,67,98,188,155,229,67,4,54,122,67,212,162,226,67,172,221,121,67,148,166,223,67,252,32,122,67,98,36,137,222,67,36,58,122,67,180,157,221,67,108,82,122,67,84,155,221,67,244,86,122,
        67,98,196,150,221,67,172,95,122,67,148,52,221,67,68,75,126,67,204,237,220,67,234,161,128,67,108,140,195,220,67,26,133,129,67,108,28,141,221,67,122,179,130,67,98,100,175,222,67,238,102,132,67,220,36,223,67,234,211,132,67,60,217,223,67,246,212,132,67,98,
        140,6,224,67,246,212,132,67,204,82,224,67,234,194,132,67,180,130,224,67,70,172,132,67,99,109,112,88,189,66,18,194,131,67,98,176,129,189,66,154,176,131,67,176,131,190,66,106,232,130,67,176,149,191,66,62,5,130,67,108,208,135,193,66,46,104,128,67,108,240,
        184,192,66,86,49,128,67,98,176,162,189,66,180,191,126,67,240,154,183,66,236,201,126,67,112,101,180,66,198,59,128,67,98,176,95,176,66,18,73,129,67,48,107,177,66,166,10,131,67,144,115,182,66,70,176,131,67,98,48,210,184,66,66,254,131,67,176,178,188,66,70,
        8,132,67,144,88,189,66,18,194,131,67,99,109,52,185,206,67,114,141,130,67,98,124,203,206,67,38,123,130,67,204,222,206,67,102,87,130,67,28,228,206,67,254,61,130,67,98,68,28,207,67,6,49,129,67,220,52,207,67,30,211,128,67,76,89,207,67,22,126,128,67,98,44,
        131,207,67,114,28,128,67,204,144,207,67,156,146,127,67,236,118,207,67,156,146,127,67,98,188,111,207,67,156,146,127,67,156,237,206,67,52,207,127,67,196,85,206,67,162,12,128,67,98,164,37,205,67,198,86,128,67,212,205,204,67,226,135,128,67,172,231,204,67,
        78,217,128,67,98,252,7,205,67,42,63,129,67,84,76,206,67,178,174,130,67,228,133,206,67,178,174,130,67,98,196,143,206,67,178,174,130,67,228,166,206,67,186,159,130,67,52,185,206,67,114,141,130,67,99,109,244,239,200,67,134,71,128,67,98,196,123,201,67,22,
        11,128,67,252,240,201,67,52,174,127,67,108,244,201,67,12,168,127,67,98,220,247,201,67,212,161,127,67,116,170,201,67,124,167,126,67,108,72,201,67,148,123,125,67,98,76,13,200,67,188,183,121,67,108,72,199,67,132,214,118,67,156,176,198,67,28,198,115,67,98,
        228,65,198,67,212,137,113,67,156,213,197,67,20,102,110,67,156,213,197,67,84,108,109,67,108,156,213,197,67,172,141,108,67,108,116,42,197,67,52,51,109,67,98,84,204,196,67,52,142,109,67,148,116,196,67,220,223,109,67,108,103,196,67,156,232,109,67,98,204,
        86,196,67,164,243,109,67,68,93,196,67,156,94,110,67,132,124,196,67,140,72,111,67,98,164,205,196,67,172,166,113,67,20,225,196,67,164,252,114,67,140,225,196,67,252,56,118,67,98,140,225,196,67,12,225,120,67,116,218,196,67,212,163,121,67,180,177,196,67,156,
        32,123,67,98,76,124,196,67,12,19,125,67,148,37,196,67,60,14,127,67,220,194,195,67,170,74,128,67,108,44,130,195,67,218,202,128,67,108,172,214,195,67,134,38,129,67,98,36,5,196,67,242,88,129,67,100,77,196,67,222,177,129,67,68,119,196,67,34,236,129,67,108,
        92,195,196,67,14,86,130,67,108,140,90,198,67,190,133,129,67,98,132,58,199,67,42,19,129,67,28,100,200,67,250,131,128,67,236,239,200,67,134,71,128,67,99,109,80,44,135,66,54,51,128,67,98,48,176,141,66,108,235,127,67,176,44,141,66,94,2,128,67,112,102,141,
        66,244,51,127,67,98,48,245,141,66,244,48,125,67,112,187,144,66,60,151,121,67,240,92,147,66,180,118,119,67,98,176,51,150,66,28,43,117,67,144,100,155,66,12,84,114,67,16,13,159,66,140,17,113,67,98,80,14,160,66,244,184,112,67,16,193,160,66,228,107,112,67,
        80,154,160,66,52,102,112,67,98,240,11,160,66,100,81,112,67,128,5,122,66,228,171,110,67,64,218,121,66,252,184,110,67,98,160,186,121,66,148,194,110,67,80,224,128,66,62,62,128,67,16,8,129,66,222,109,128,67,98,48,12,129,66,254,112,128,67,144,98,129,66,230,
        110,128,67,112,203,129,66,74,105,128,67,98,80,52,130,66,146,99,128,67,240,159,132,66,78,75,128,67,80,44,135,66,62,51,128,67,99,109,132,107,219,67,12,132,127,67,98,132,120,219,67,188,100,127,67,92,140,219,67,132,218,126,67,164,151,219,67,228,80,126,67,
        98,236,162,219,67,68,199,125,67,36,184,219,67,116,240,124,67,220,198,219,67,124,115,124,67,98,60,225,219,67,252,146,123,67,220,224,219,67,36,143,123,67,188,166,219,67,44,43,123,67,98,196,98,219,67,52,182,122,67,228,45,219,67,180,174,122,67,84,136,218,
        67,132,2,123,67,98,148,70,218,67,212,35,123,67,52,217,217,67,84,76,123,67,76,149,217,67,140,92,123,67,98,92,81,217,67,196,108,123,67,92,21,217,67,52,145,123,67,244,15,217,67,132,173,123,67,98,220,5,217,67,4,226,123,67,84,88,218,67,188,176,126,67,140,
        204,218,67,156,93,127,67,98,116,21,219,67,28,202,127,67,68,73,219,67,156,214,127,67,132,107,219,67,4,132,127,67,99,109,108,36,206,67,108,240,124,67,108,20,73,208,67,164,166,123,67,108,28,235,208,67,228,133,120,67,98,60,68,209,67,124,205,118,67,36,158,
        209,67,4,37,117,67,228,178,209,67,180,214,116,67,108,164,216,209,67,68,72,116,67,108,84,8,209,67,124,178,114,67,108,4,56,208,67,172,28,113,67,108,196,133,206,67,132,24,113,67,98,92,234,204,67,140,20,113,67,148,201,204,67,28,14,113,67,172,21,204,67,12,
        159,112,67,98,212,53,202,67,156,118,111,67,60,2,201,67,92,95,109,67,12,130,200,67,172,101,106,67,98,140,104,200,67,36,206,105,67,148,73,200,67,116,75,105,67,60,61,200,67,60,67,105,67,98,228,48,200,67,236,58,105,67,180,221,199,67,228,169,105,67,100,132,
        199,67,148,57,106,67,98,12,43,199,67,76,201,106,67,180,203,198,67,12,92,107,67,124,176,198,67,180,127,107,67,98,36,112,198,67,12,212,107,67,132,110,198,67,148,191,108,67,156,171,198,67,140,77,110,67,98,28,45,199,67,36,153,113,67,252,39,201,67,196,110,
        120,67,148,241,202,67,204,13,125,67,98,148,114,203,67,84,91,126,67,148,124,203,67,100,107,126,67,116,188,203,67,108,83,126,67,98,100,225,203,67,124,69,126,67,140,246,204,67,212,165,125,67,84,36,206,67,108,240,124,67,99,109,32,43,112,66,140,16,116,67,
        98,32,99,108,66,244,102,110,67,0,12,107,66,236,7,108,67,64,125,107,66,196,201,107,67,98,192,222,107,66,84,148,107,67,160,251,117,66,204,11,106,67,144,213,130,66,180,209,103,67,98,224,220,137,66,212,210,101,67,16,157,143,66,140,38,100,67,16,157,143,66,
        20,26,100,67,98,16,157,143,66,148,13,100,67,192,112,137,66,100,159,99,67,64,229,129,66,44,37,99,67,98,128,179,116,66,252,170,98,67,64,229,103,66,180,59,98,67,128,85,103,66,236,45,98,67,98,192,197,102,66,44,32,98,67,96,10,102,66,84,207,97,67,224,180,101,
        66,84,122,97,67,98,96,95,101,66,92,37,97,67,96,219,99,66,252,152,93,67,128,86,98,66,196,151,89,67,98,192,209,96,66,140,150,85,67,96,110,95,66,28,51,82,67,224,64,95,66,44,16,82,67,98,32,19,95,66,52,237,81,67,128,230,89,66,76,241,84,67,128,192,83,66,244,
        195,88,67,98,96,154,77,66,148,150,92,67,192,16,72,66,60,245,95,67,192,113,71,66,244,64,96,67,98,64,132,70,66,44,178,96,67,192,236,69,66,164,202,96,67,32,30,68,66,164,202,96,67,98,192,232,66,66,164,202,96,67,96,254,54,66,28,88,96,67,160,163,41,66,28,204,
        95,67,98,224,72,28,66,28,64,95,67,224,68,17,66,60,211,94,67,64,41,17,66,52,218,94,67,98,32,13,17,66,68,225,94,67,0,116,21,66,164,32,96,67,224,240,26,66,44,160,97,67,98,128,147,50,66,28,20,104,67,96,163,54,66,188,61,105,67,96,163,54,66,164,141,105,67,
        98,96,163,54,66,148,187,105,67,160,144,48,66,236,253,108,67,128,36,41,66,188,203,112,67,98,96,184,33,66,132,153,116,67,96,188,27,66,124,188,119,67,0,216,27,66,28,196,119,67,98,96,243,27,66,204,203,119,67,64,166,38,66,132,35,118,67,192,157,51,66,124,21,
        116,67,98,32,253,65,66,92,206,113,67,224,212,75,66,4,89,112,67,128,212,76,66,4,89,112,67,98,32,187,77,66,4,89,112,67,96,253,78,66,4,123,112,67,96,160,79,66,140,164,112,67,98,160,67,80,66,28,206,112,67,64,244,88,66,188,114,115,67,128,240,98,66,44,132,
        118,67,98,160,236,108,66,164,149,121,67,128,45,117,66,228,18,124,67,0,72,117,66,68,12,124,67,98,128,98,117,66,156,5,124,67,128,21,115,66,4,110,120,67,64,43,112,66,148,16,116,67,99,109,96,222,98,66,172,79,123,67,98,160,95,92,66,204,103,121,67,224,247,
        74,66,28,124,116,67,192,184,74,66,28,124,116,67,98,192,61,74,66,28,124,116,67,224,151,28,66,92,243,122,67,96,220,27,66,84,31,123,67,98,160,125,27,66,148,53,123,67,160,227,30,66,140,71,123,67,160,34,36,66,20,75,123,67,98,64,8,41,66,100,78,123,67,96,211,
        53,66,196,93,123,67,224,144,64,66,92,109,123,67,98,192,106,101,66,212,162,123,67,224,253,99,66,20,164,123,67,32,222,98,66,172,79,123,67,99,109,20,109,211,67,236,70,122,67,98,92,175,211,67,76,24,122,67,4,2,212,67,44,242,121,67,172,36,212,67,44,242,121,
        67,98,100,111,212,67,44,242,121,67,164,206,212,67,220,162,121,67,52,210,212,67,180,97,121,67,98,140,211,212,67,76,72,121,67,60,211,212,67,236,22,121,67,188,208,212,67,244,243,120,67,98,68,204,212,67,100,171,120,67,252,175,211,67,132,120,118,67,204,143,
        211,67,132,120,118,67,98,44,46,211,67,132,120,118,67,28,193,210,67,20,143,119,67,220,146,210,67,100,254,120,67,98,36,125,210,67,204,170,121,67,124,126,210,67,156,214,121,67,84,156,210,67,140,49,122,67,98,68,198,210,67,148,177,122,67,100,211,210,67,236,
        178,122,67,12,109,211,67,236,70,122,67,99,109,44,29,217,67,140,75,120,67,98,196,69,219,67,36,199,119,67,100,128,219,67,196,175,119,67,228,205,219,67,140,57,119,67,98,76,120,220,67,156,53,118,67,156,151,220,67,108,158,116,67,84,157,220,67,100,176,108,
        67,98,68,161,220,67,124,59,103,67,100,159,220,67,220,240,102,67,84,113,220,67,180,159,101,67,98,124,65,220,67,220,65,100,67,156,239,219,67,116,245,98,67,92,144,219,67,100,14,98,67,98,20,107,219,67,236,179,97,67,124,70,219,67,228,143,97,67,220,255,218,
        67,244,127,97,67,108,76,161,218,67,164,106,97,67,108,12,36,218,67,220,204,98,67,98,92,174,217,67,204,25,100,67,164,166,217,67,188,62,100,67,116,163,217,67,20,50,101,67,98,68,161,217,67,108,216,101,67,124,144,217,67,20,114,102,67,132,116,217,67,196,223,
        102,67,98,108,37,217,67,228,21,104,67,132,75,216,67,204,63,105,67,60,128,215,67,188,139,105,67,98,60,63,215,67,12,164,105,67,36,243,214,67,44,252,105,67,180,120,214,67,228,188,106,67,98,156,229,213,67,140,164,107,67,76,179,213,67,228,24,108,67,172,30,
        213,67,124,221,109,67,98,12,45,212,67,116,189,112,67,132,2,212,67,188,203,113,67,244,72,212,67,204,43,115,67,98,196,100,212,67,228,182,115,67,52,182,212,67,132,87,116,67,188,224,213,67,252,79,118,67,98,236,235,214,67,140,19,120,67,188,97,215,67,140,193,
        120,67,20,128,215,67,92,181,120,67,98,100,151,215,67,252,171,120,67,68,81,216,67,92,124,120,67,36,29,217,67,132,75,120,67,99,109,76,184,210,67,252,75,112,67,98,204,230,210,67,76,24,112,67,156,88,211,67,92,4,111,67,204,74,211,67,188,232,110,67,98,212,
        70,211,67,220,224,110,67,140,240,210,67,228,32,111,67,236,138,210,67,244,118,111,67,98,116,219,209,67,132,11,112,67,164,212,209,67,20,22,112,67,76,3,210,67,220,73,112,67,98,28,65,210,67,116,142,112,67,228,123,210,67,36,143,112,67,76,184,210,67,244,75,
        112,67,99,109,156,112,206,67,196,243,110,67,98,156,112,206,67,180,209,110,67,140,149,204,67,108,153,108,67,236,177,203,67,36,171,107,67,98,52,157,202,67,132,137,106,67,20,164,201,67,44,172,105,67,228,148,201,67,132,202,105,67,98,84,133,201,67,172,233,
        105,67,76,220,201,67,92,47,107,67,196,29,202,67,4,203,107,67,98,132,140,202,67,92,210,108,67,228,125,203,67,204,234,109,67,164,101,204,67,92,113,110,67,98,116,255,204,67,172,202,110,67,164,112,206,67,188,38,111,67,164,112,206,67,196,243,110,67,99,109,
        68,114,209,67,164,216,109,67,98,108,147,210,67,20,18,109,67,228,94,212,67,60,51,107,67,116,119,212,67,220,178,106,67,98,76,123,212,67,124,158,106,67,236,169,212,67,220,23,106,67,244,222,212,67,204,135,105,67,98,164,92,213,67,84,50,104,67,132,103,213,
        67,76,219,103,67,244,52,213,67,92,214,102,67,98,84,220,212,67,180,12,101,67,212,28,213,67,196,19,99,67,236,214,213,67,204,226,97,67,98,116,104,214,67,76,244,96,67,76,137,214,67,196,229,96,67,236,24,216,67,172,226,96,67,98,244,65,217,67,100,224,96,67,
        132,139,217,67,228,211,96,67,212,165,217,67,76,159,96,67,98,52,212,217,67,156,66,96,67,140,175,218,67,60,30,93,67,84,164,218,67,236,249,92,67,98,36,159,218,67,20,233,92,67,20,119,218,67,44,206,92,67,68,75,218,67,236,189,92,67,98,244,186,217,67,68,136,
        92,67,12,229,216,67,100,132,91,67,52,97,216,67,148,106,90,67,98,156,57,215,67,148,242,87,67,172,235,214,67,28,208,84,67,228,147,215,67,84,40,82,67,98,180,176,215,67,228,179,81,67,228,195,215,67,212,75,81,67,140,190,215,67,36,65,81,67,98,60,185,215,67,
        132,54,81,67,252,83,215,67,116,86,81,67,172,221,214,67,76,136,81,67,98,188,154,213,67,84,16,82,67,156,70,212,67,228,115,83,67,76,144,211,67,4,252,84,67,98,236,51,210,67,84,233,87,67,100,254,209,67,20,21,90,67,52,231,210,67,36,216,91,67,98,108,94,211,
        67,52,191,92,67,108,239,211,67,172,80,93,67,148,57,213,67,156,44,94,67,98,12,44,214,67,36,206,94,67,148,44,214,67,204,206,94,67,20,174,213,67,132,186,94,67,98,36,34,212,67,20,123,94,67,188,151,210,67,124,207,92,67,4,27,210,67,116,218,90,67,98,124,221,
        209,67,84,227,89,67,108,231,209,67,28,255,87,67,180,47,210,67,100,233,86,67,98,220,206,210,67,220,133,84,67,60,5,212,67,204,75,82,67,132,106,213,67,220,250,80,67,98,108,45,214,67,12,67,80,67,228,211,214,67,196,236,79,67,108,179,215,67,204,203,79,67,98,
        84,95,216,67,108,178,79,67,44,104,216,67,236,171,79,67,220,166,216,67,100,25,79,67,98,76,254,217,67,212,246,75,67,4,223,219,67,156,229,72,67,92,215,221,67,116,159,70,67,98,172,62,222,67,52,40,70,67,52,147,222,67,28,194,69,67,52,147,222,67,148,188,69,
        67,98,52,147,222,67,180,170,69,67,212,78,221,67,132,27,70,67,236,187,220,67,140,96,70,67,98,244,102,218,67,20,121,71,67,188,157,214,67,140,207,74,67,228,175,211,67,84,90,78,67,98,180,61,208,67,28,133,82,67,132,113,206,67,140,46,85,67,148,178,204,67,4,
        180,88,67,108,4,211,203,67,244,118,90,67,108,156,173,203,67,100,217,91,67,98,196,83,203,67,84,44,95,67,68,147,202,67,60,206,98,67,76,172,201,67,164,136,101,67,98,148,62,201,67,116,212,102,67,164,38,201,67,156,105,103,67,28,76,201,67,188,223,103,67,98,
        180,85,201,67,244,253,103,67,68,183,201,67,52,67,104,67,236,36,202,67,172,121,104,67,98,68,182,203,67,4,65,105,67,140,209,205,67,20,30,107,67,188,163,207,67,60,86,109,67,98,60,24,208,67,44,228,109,67,188,132,208,67,84,89,110,67,212,148,208,67,132,90,
        110,67,98,236,164,208,67,156,92,110,67,140,8,209,67,76,33,110,67,60,114,209,67,180,216,109,67,99,109,48,97,176,66,20,20,109,67,98,112,144,179,66,100,171,108,67,16,101,180,66,204,161,108,67,16,79,186,66,212,160,108,67,98,16,46,193,66,188,158,108,67,176,
        149,195,66,204,201,108,67,240,211,200,66,236,158,109,67,98,144,51,202,66,188,214,109,67,240,93,203,66,44,255,109,67,240,106,203,66,180,248,109,67,98,176,154,203,66,220,224,109,67,48,145,204,66,228,90,107,67,208,112,204,66,172,74,107,67,98,144,94,204,
        66,76,65,107,67,208,30,193,66,228,216,105,67,240,113,179,66,132,41,104,67,108,176,148,154,66,36,25,101,67,108,240,137,145,66,164,215,103,67,98,208,144,140,66,4,90,105,67,112,97,135,66,100,238,106,67,80,4,134,66,60,90,107,67,108,96,137,131,66,92,30,108,
        67,108,48,187,150,66,36,11,109,67,98,16,7,168,66,132,224,109,67,176,18,170,66,84,242,109,67,48,106,171,66,28,191,109,67,98,240,59,172,66,228,159,109,67,240,119,174,66,228,82,109,67,48,97,176,66,12,20,109,67,99,109,176,235,205,66,172,24,104,67,98,112,
        15,206,66,4,212,103,67,112,180,204,66,84,80,99,67,16,114,204,66,116,47,99,67,98,80,100,204,66,156,40,99,67,80,116,203,66,212,102,99,67,144,92,202,66,172,185,99,67,98,240,68,201,66,132,12,100,67,144,161,199,66,236,118,100,67,144,184,198,66,28,166,100,
        67,98,144,207,197,66,76,213,100,67,112,148,196,66,140,41,101,67,112,252,195,66,84,97,101,67,98,144,100,195,66,28,153,101,67,80,13,194,66,12,248,101,67,240,1,193,66,92,52,102,67,108,208,27,191,66,244,161,102,67,108,208,228,197,66,196,128,103,67,98,48,
        160,201,66,76,251,103,67,16,236,204,66,84,97,104,67,48,56,205,66,132,99,104,67,98,80,132,205,66,148,101,104,67,48,213,205,66,4,68,104,67,208,235,205,66,172,24,104,67,99,109,240,191,175,66,12,26,100,67,98,144,6,173,66,236,96,98,67,240,99,172,66,180,173,
        96,67,16,95,173,66,100,188,93,67,98,240,187,173,66,156,165,92,67,80,0,174,66,12,190,91,67,208,246,173,66,196,185,91,67,98,208,163,173,66,44,148,91,67,208,120,148,66,228,8,86,67,16,33,148,66,228,8,86,67,98,112,225,147,66,228,8,86,67,176,147,146,66,92,
        73,86,67,80,59,145,66,52,152,86,67,98,0,63,137,66,252,107,88,67,80,87,131,66,68,244,87,67,64,2,129,66,68,79,85,67,98,192,73,128,66,12,126,84,67,48,55,128,66,100,8,84,67,144,125,128,66,228,3,82,67,98,0,141,128,66,244,145,81,67,176,87,128,66,172,129,81,
        67,128,65,117,66,220,59,80,67,98,192,188,110,66,12,130,79,67,160,82,105,66,60,239,78,67,0,57,105,66,172,245,78,67,98,224,171,104,66,236,24,79,67,96,198,106,66,148,140,83,67,160,236,109,66,108,233,88,67,98,128,229,111,66,236,68,92,67,0,152,113,66,52,8,
        95,67,0,178,113,66,44,13,95,67,98,128,204,113,66,44,18,95,67,112,225,128,66,60,197,95,67,224,149,138,66,44,155,96,67,98,80,74,148,66,12,113,97,67,48,124,156,66,100,52,98,67,208,203,156,66,76,77,98,67,98,48,59,157,66,28,112,98,67,240,156,175,66,44,214,
        100,67,240,204,176,66,148,233,100,67,98,16,241,176,66,220,235,100,67,208,119,176,66,116,142,100,67,176,191,175,66,4,26,100,67,99,109,48,219,185,66,108,7,98,67,98,240,127,189,66,36,92,97,67,240,235,193,66,252,102,96,67,240,235,193,66,60,72,96,67,98,240,
        235,193,66,12,51,96,67,112,38,185,66,172,40,94,67,144,31,182,66,148,137,93,67,98,144,141,181,66,164,107,93,67,208,66,181,66,52,232,94,67,144,167,181,66,148,236,95,67,98,80,241,181,66,108,171,96,67,176,61,183,66,76,114,98,67,112,127,183,66,76,114,98,67,
        98,112,139,183,66,76,114,98,67,48,155,184,66,52,66,98,67,80,219,185,66,108,7,98,67,99,109,208,184,204,66,204,38,93,67,98,208,225,206,66,116,108,92,67,80,230,208,66,20,187,91,67,80,52,209,66,148,156,91,67,98,176,15,210,66,212,70,91,67,208,119,210,66,4,
        88,88,67,48,120,210,66,148,126,82,67,108,112,120,210,66,188,97,77,67,108,176,30,206,66,36,35,74,67,98,16,186,203,66,84,90,72,67,80,166,201,66,108,211,70,67,240,128,201,66,124,190,70,67,98,112,91,201,66,148,169,70,67,48,221,200,66,164,4,71,67,80,104,200,
        66,212,136,71,67,98,80,243,199,66,4,13,72,67,16,140,198,66,60,98,73,67,208,73,197,66,12,127,74,67,98,48,190,193,66,84,161,77,67,16,205,191,66,132,170,79,67,112,119,190,66,60,166,81,67,98,240,202,189,66,140,166,82,67,80,40,188,66,212,178,84,67,16,213,
        186,66,76,51,86,67,98,144,217,183,66,108,148,89,67,48,43,183,66,20,122,90,67,112,120,183,66,124,161,90,67,98,112,200,183,66,116,202,90,67,48,240,199,66,196,112,94,67,48,107,200,66,180,117,94,67,98,240,159,200,66,204,119,94,67,144,143,202,66,28,225,93,
        67,176,184,204,66,204,38,93,67,99,109,100,121,222,67,204,64,91,67,98,92,231,224,67,252,133,90,67,252,37,227,67,252,47,88,67,148,99,228,67,4,25,85,67,98,252,134,229,67,68,67,82,67,204,214,229,67,116,200,79,67,228,113,229,67,92,179,76,67,98,4,55,229,67,
        204,230,74,67,36,213,228,67,44,144,73,67,12,41,228,67,244,51,72,67,98,196,135,227,67,156,237,70,67,236,218,226,67,228,36,70,67,36,24,226,67,180,205,69,67,108,132,165,225,67,100,154,69,67,108,212,111,224,67,36,200,70,67,98,68,100,222,67,76,198,72,67,244,
        143,220,67,12,22,75,67,148,25,219,67,100,134,77,67,98,92,113,218,67,236,158,78,67,36,163,217,67,84,52,80,67,188,178,217,67,188,71,80,67,98,212,183,217,67,44,78,80,67,188,251,217,67,172,146,80,67,140,73,218,67,20,224,80,67,98,252,214,219,67,124,107,82,
        67,156,147,220,67,12,199,85,67,44,65,220,67,76,215,89,67,98,36,53,220,67,196,110,90,67,76,38,220,67,12,12,91,67,28,32,220,67,212,52,91,67,98,12,22,220,67,204,119,91,67,116,40,220,67,244,126,91,67,180,223,220,67,244,126,91,67,98,60,79,221,67,244,126,91,
        67,156,7,222,67,252,98,91,67,108,121,222,67,204,64,91,67,99,109,236,69,219,67,172,211,89,67,98,164,163,219,67,108,106,87,67,92,89,219,67,212,251,84,67,212,122,218,67,12,46,83,67,98,108,37,218,67,204,124,82,67,212,107,217,67,12,145,81,67,180,53,217,67,
        12,145,81,67,98,36,43,217,67,12,145,81,67,100,249,216,67,52,51,82,67,36,199,216,67,100,249,82,67,98,228,72,216,67,116,235,84,67,228,62,216,67,188,244,85,67,20,156,216,67,100,111,87,67,98,4,223,216,67,84,127,88,67,244,74,217,67,4,89,89,67,252,243,217,
        67,252,36,90,67,98,172,67,218,67,36,133,90,67,4,209,218,67,172,247,90,67,116,254,218,67,228,252,90,67,98,172,12,219,67,252,254,90,67,212,44,219,67,204,120,90,67,236,69,219,67,172,211,89,67,99,109,176,191,176,66,100,217,86,67,98,48,62,179,66,76,239,82,
        67,48,126,182,66,116,43,79,67,112,56,188,66,188,143,73,67,98,16,59,190,66,204,151,71,67,240,206,191,66,124,251,69,67,16,186,191,66,124,251,69,67,98,208,129,191,66,124,251,69,67,208,255,184,66,148,118,72,67,80,14,175,66,76,86,76,67,98,112,59,170,66,108,
        55,78,67,240,137,163,66,188,187,80,67,208,46,160,66,20,238,81,67,108,144,20,154,66,20,27,84,67,108,112,13,155,66,220,85,84,67,98,16,126,160,66,180,158,85,67,208,246,174,66,244,206,88,67,208,55,175,66,52,199,88,67,98,208,101,175,66,204,193,88,67,48,22,
        176,66,124,227,87,67,176,191,176,66,100,217,86,67,99,109,248,1,123,67,68,234,78,67,108,152,186,123,67,76,67,78,67,108,200,251,122,67,172,186,76,67,98,56,40,122,67,52,7,75,67,136,122,121,67,172,241,72,67,248,55,121,67,108,78,71,67,98,152,255,120,67,132,
        235,69,67,216,255,120,67,108,186,66,67,248,55,121,67,68,89,65,67,98,40,79,122,67,84,136,58,67,120,41,127,67,172,212,52,67,60,224,130,67,120,159,50,67,98,220,41,136,67,120,20,47,67,148,213,141,67,244,152,53,67,252,247,142,67,108,133,64,67,108,100,29,143,
        67,140,237,65,67,108,220,120,143,67,212,250,65,67,98,44,171,143,67,36,2,66,67,84,235,143,67,236,252,65,67,108,7,144,67,148,238,65,67,98,100,54,144,67,4,215,65,67,92,57,144,67,132,198,65,67,84,44,144,67,140,34,65,67,98,228,120,143,67,148,77,56,67,252,
        5,140,67,44,66,49,67,140,166,135,67,40,186,47,67,98,148,176,134,67,12,100,47,67,172,248,132,67,12,100,47,67,180,2,132,67,40,186,47,67,98,236,39,128,67,184,19,49,67,88,204,121,67,4,223,54,67,136,162,119,67,172,77,62,67,98,104,116,118,67,156,91,66,67,104,
        116,118,67,244,75,70,67,136,162,119,67,220,89,74,67,98,88,63,120,67,140,116,76,67,104,196,121,67,132,152,79,67,168,41,122,67,220,146,79,67,98,232,58,122,67,220,146,79,67,248,155,122,67,252,69,79,67,120,1,123,67,44,234,78,67,99,109,140,209,237,67,124,
        146,76,67,98,228,20,238,67,52,72,76,67,196,52,238,67,196,255,75,67,156,83,238,67,212,106,75,67,108,132,124,238,67,140,165,74,67,108,164,68,238,67,28,68,73,67,98,212,240,237,67,196,49,71,67,228,2,238,67,52,66,71,67,244,24,237,67,124,51,72,67,98,20,170,
        236,67,220,165,72,67,68,41,236,67,188,73,73,67,188,250,235,67,172,159,73,67,98,124,176,235,67,196,40,74,67,20,166,235,67,4,83,74,67,20,166,235,67,180,247,74,67,98,20,166,235,67,4,224,75,67,12,209,235,67,84,96,76,67,116,66,236,67,196,202,76,67,98,148,
        177,236,67,252,50,77,67,236,84,237,67,252,27,77,67,140,209,237,67,124,146,76,67,99,109,216,190,127,67,140,106,75,67,108,196,141,128,67,172,143,74,67,108,36,69,128,67,76,85,73,67,98,216,206,127,67,164,191,71,67,88,129,127,67,196,72,70,67,88,129,127,67,
        220,83,68,67,98,88,129,127,67,220,79,66,67,24,209,127,67,60,222,64,67,124,79,128,67,156,40,63,67,98,252,228,128,67,4,173,60,67,76,232,129,67,52,151,58,67,204,25,131,67,44,106,57,67,98,76,21,132,67,100,114,56,67,228,170,132,67,228,46,56,67,220,212,133,
        67,228,46,56,67,98,204,254,134,67,228,46,56,67,92,148,135,67,92,114,56,67,220,143,136,67,44,106,57,67,98,188,34,138,67,28,247,58,67,100,112,139,67,84,71,62,67,188,201,139,67,188,157,65,67,98,204,217,139,67,28,55,66,67,244,233,139,67,44,186,66,67,164,
        237,139,67,228,192,66,67,98,4,245,139,67,100,206,66,67,204,128,141,67,116,92,66,67,228,137,141,67,60,74,66,67,98,252,149,141,67,28,50,66,67,204,80,141,67,52,240,63,67,180,38,141,67,188,13,63,67,98,180,64,139,67,132,213,52,67,212,1,133,67,84,131,49,67,
        172,205,128,67,236,124,56,67,98,232,25,125,67,204,57,60,67,40,15,123,67,252,231,66,67,88,179,124,67,204,137,72,67,98,216,7,125,67,244,171,73,67,8,38,126,67,100,69,76,67,72,78,126,67,100,69,76,67,98,72,89,126,67,100,69,76,67,24,255,126,67,236,226,75,67,
        216,190,127,67,148,106,75,67,99,109,44,84,162,67,220,177,73,67,108,60,86,162,67,212,176,72,67,108,228,195,161,67,28,66,73,67,108,140,49,161,67,108,211,73,67,108,4,180,161,67,116,153,74,67,98,220,74,162,67,100,126,75,67,108,80,162,67,84,118,75,67,36,84,
        162,67,228,177,73,67,99,109,40,82,67,67,100,145,74,67,98,120,95,67,67,4,92,74,67,216,126,67,67,12,124,73,67,168,151,67,67,164,159,72,67,98,184,202,67,67,140,219,70,67,72,252,67,67,100,5,70,67,232,74,68,67,100,154,69,67,98,216,127,68,67,60,82,69,67,248,
        126,71,67,100,119,68,67,72,18,75,67,92,171,67,67,98,40,52,76,67,188,106,67,67,200,166,76,67,236,63,67,67,40,152,76,67,188,25,67,67,98,88,136,76,67,140,240,66,67,184,41,75,67,244,41,67,67,104,218,70,67,188,10,68,67,98,120,189,67,67,4,173,68,67,248,42,
        65,67,148,55,69,67,8,35,65,67,172,62,69,67,98,184,9,65,67,76,85,69,67,56,39,66,67,92,134,74,67,40,81,66,67,124,189,74,67,98,168,150,66,67,164,24,75,67,40,55,67,67,28,253,74,67,56,82,67,67,100,145,74,67,99,109,40,75,115,67,212,135,74,67,98,8,52,115,67,
        252,14,74,67,88,1,115,67,20,226,73,67,152,190,114,67,84,11,74,67,98,120,161,114,67,108,29,74,67,104,179,114,67,12,76,74,67,88,240,114,67,236,140,74,67,98,168,101,115,67,196,9,75,67,8,100,115,67,228,9,75,67,40,75,115,67,236,135,74,67,99,109,80,101,211,
        66,44,44,71,67,98,112,223,212,66,92,155,70,67,112,64,218,66,188,45,69,67,240,44,221,66,92,147,68,67,98,112,247,222,66,196,52,68,67,48,111,224,66,140,216,67,67,176,111,224,66,100,198,67,67,98,176,111,224,66,188,139,67,67,16,162,182,66,116,176,30,67,240,
        253,181,66,200,91,30,67,98,80,97,181,66,24,11,30,67,208,71,181,66,56,15,30,67,176,252,174,66,228,126,31,67,98,144,223,160,66,116,183,34,67,16,130,158,66,252,69,35,67,144,132,158,66,140,95,35,67,98,144,132,158,66,84,110,35,67,48,250,166,66,160,187,41,
        67,208,77,177,66,224,96,49,67,108,80,20,196,66,148,71,63,67,108,16,143,198,66,28,124,63,67,98,48,236,199,66,252,152,63,67,48,51,201,66,140,184,63,67,144,101,201,66,52,194,63,67,98,176,22,202,66,28,228,63,67,48,233,202,66,164,164,65,67,48,233,202,66,92,
        252,66,67,108,48,233,202,66,172,78,68,67,108,176,155,206,66,4,17,71,67,108,48,78,210,66,100,211,73,67,108,144,145,210,66,116,155,72,67,98,176,191,210,66,28,197,71,67,16,2,211,66,44,82,71,67,112,101,211,66,36,44,71,67,99,109,228,117,130,67,132,134,72,
        67,108,236,111,131,67,228,151,71,67,108,76,58,131,67,44,191,70,67,98,68,12,131,67,52,5,70,67,172,4,131,67,76,173,69,67,172,4,131,67,220,83,68,67,98,172,4,131,67,132,234,66,67,4,11,131,67,44,168,66,67,220,66,131,67,220,203,65,67,98,252,142,131,67,100,
        159,64,67,172,19,132,67,172,163,63,67,52,179,132,67,60,17,63,67,98,60,21,133,67,60,183,62,67,44,75,133,67,212,162,62,67,116,215,133,67,172,162,62,67,98,172,110,134,67,172,162,62,67,92,148,134,67,20,179,62,67,140,14,135,67,76,43,63,67,98,116,244,135,67,
        172,13,64,67,172,175,136,67,60,20,66,67,172,175,136,67,196,174,67,67,98,172,175,136,67,60,21,68,67,44,183,136,67,164,42,68,67,196,212,136,67,244,24,68,67,98,188,0,137,67,164,254,67,67,44,207,138,67,60,43,67,67,196,220,138,67,60,43,67,67,98,244,225,138,
        67,60,43,67,67,204,221,138,67,52,209,66,67,124,211,138,67,44,99,66,67,98,124,138,138,67,100,85,63,67,12,122,137,67,212,146,60,67,220,16,136,67,116,58,59,67,98,252,59,135,67,140,111,58,67,172,184,134,67,12,51,58,67,236,212,133,67,12,51,58,67,98,84,238,
        132,67,12,51,58,67,228,107,132,67,4,112,58,67,60,150,131,67,180,63,59,67,98,60,37,129,67,44,159,61,67,28,20,128,67,180,130,67,67,92,47,129,67,132,129,72,67,98,12,77,129,67,132,7,73,67,108,106,129,67,36,117,73,67,156,112,129,67,36,117,73,67,98,220,118,
        129,67,36,117,73,67,108,236,129,67,196,9,73,67,244,117,130,67,140,134,72,67,99,109,4,23,133,67,76,98,70,67,98,12,174,133,67,188,247,69,67,60,151,134,67,204,94,69,67,52,29,135,67,84,14,69,67,98,52,163,135,67,228,189,68,67,140,27,136,67,124,116,68,67,180,
        40,136,67,52,107,68,67,98,20,77,136,67,156,81,68,67,36,34,136,67,204,184,66,67,228,228,135,67,4,230,65,67,98,12,129,135,67,20,142,64,67,68,156,134,67,44,135,63,67,228,212,133,67,44,135,63,67,98,36,11,133,67,44,135,63,67,92,41,132,67,108,141,64,67,76,
        193,131,67,148,240,65,67,98,172,132,131,67,124,191,66,67,140,106,131,67,132,37,68,67,164,129,131,67,124,89,69,67,98,140,143,131,67,20,19,70,67,84,211,131,67,252,35,71,67,124,243,131,67,252,35,71,67,98,212,252,131,67,252,35,71,67,12,128,132,67,212,204,
        70,67,12,23,133,67,76,98,70,67,99,109,148,242,160,67,252,241,70,67,98,68,158,161,67,28,139,70,67,244,60,162,67,4,119,68,67,140,38,162,67,116,233,66,67,98,252,23,162,67,220,230,65,67,220,186,161,67,180,191,64,67,44,75,161,67,12,50,64,67,98,20,187,159,
        67,148,54,62,67,124,202,157,67,44,120,65,67,28,133,158,67,244,217,68,67,98,108,236,158,67,60,185,70,67,172,231,159,67,228,145,71,67,156,242,160,67,252,241,70,67,99,109,220,8,157,67,220,235,68,67,98,132,226,156,67,76,187,67,67,252,248,156,67,20,194,65,
        67,28,57,157,67,236,174,64,67,98,4,203,157,67,180,60,62,67,244,216,158,67,220,228,60,67,236,60,160,67,164,215,60,67,98,252,13,161,67,244,207,60,67,180,150,161,67,156,39,61,67,132,65,162,67,4,35,62,67,98,116,36,163,67,52,113,63,67,164,156,163,67,164,101,
        65,67,100,144,163,67,140,149,67,67,98,172,138,163,67,124,152,68,67,244,142,163,67,196,207,68,67,244,166,163,67,60,189,68,67,98,52,183,163,67,196,176,68,67,172,5,164,67,252,147,68,67,84,85,164,67,92,125,68,67,98,4,165,164,67,196,102,68,67,12,72,165,67,
        92,38,68,67,172,191,165,67,52,238,67,67,98,76,55,166,67,20,182,67,67,228,227,166,67,92,112,67,67,60,63,167,67,68,83,67,67,98,156,163,167,67,76,51,67,67,180,233,167,67,132,7,67,67,100,240,167,67,156,228,66,67,98,244,254,167,67,196,152,66,67,20,211,167,
        67,44,53,64,67,244,167,167,67,236,242,62,67,98,188,65,167,67,124,246,59,67,36,249,165,67,108,122,56,67,204,148,164,67,188,153,54,67,98,180,37,161,67,232,247,49,67,52,139,156,67,52,149,51,67,76,6,154,67,148,77,58,67,98,196,55,153,67,132,116,60,67,188,
        146,152,67,68,17,64,67,188,146,152,67,124,111,66,67,98,188,146,152,67,252,23,67,67,60,152,152,67,12,44,67,67,252,204,152,67,132,67,67,67,98,140,164,153,67,84,163,67,67,4,84,155,67,164,148,68,67,148,33,156,67,68,32,69,67,98,148,167,156,67,68,123,69,67,
        252,23,157,67,28,193,69,67,116,27,157,67,92,187,69,67,98,220,30,157,67,140,181,69,67,140,22,157,67,68,88,69,67,220,8,157,67,220,235,68,67,99,109,60,216,202,67,52,179,62,67,98,132,186,202,67,204,7,60,67,52,162,202,67,220,165,57,67,52,162,202,67,212,103,
        57,67,98,52,162,202,67,228,253,56,67,148,201,202,67,180,208,56,67,36,42,205,67,32,128,54,67,98,108,9,207,67,100,173,52,67,236,167,207,67,32,1,52,67,36,139,207,67,20,234,51,67,98,252,80,207,67,144,187,51,67,124,126,202,67,52,160,49,67,28,83,202,67,100,
        162,49,67,98,252,39,202,67,120,164,49,67,148,26,201,67,144,139,51,67,236,51,201,67,144,169,51,67,98,44,61,201,67,148,180,51,67,236,206,201,67,8,251,51,67,220,119,202,67,60,70,52,67,98,204,32,203,67,120,145,52,67,28,182,203,67,40,219,52,67,180,195,203,
        67,0,234,52,67,98,76,209,203,67,196,248,52,67,148,67,203,67,248,147,53,67,204,136,202,67,184,66,54,67,108,52,53,201,67,124,128,55,67,108,140,65,201,67,148,232,56,67,98,84,72,201,67,164,174,57,67,92,84,201,67,12,10,59,67,76,92,201,67,148,236,59,67,108,
        164,106,201,67,116,136,61,67,108,132,89,200,67,12,101,59,67,108,84,72,199,67,148,65,57,67,108,28,242,197,67,44,100,58,67,98,116,160,196,67,228,130,59,67,172,151,196,67,188,141,59,67,12,95,195,67,148,145,61,67,98,84,165,194,67,244,195,62,67,236,19,194,
        67,132,213,63,67,196,255,193,67,108,38,64,67,108,100,221,193,67,116,176,64,67,108,244,33,194,67,116,118,64,67,98,172,71,194,67,140,86,64,67,228,115,195,67,4,85,63,67,20,189,196,67,44,58,62,67,98,68,6,198,67,84,31,61,67,228,22,199,67,220,55,60,67,236,
        26,199,67,220,55,60,67,98,252,30,199,67,220,55,60,67,76,1,200,67,108,10,62,67,228,17,201,67,172,68,64,67,98,228,175,202,67,156,166,67,67,148,2,203,67,84,65,68,67,236,7,203,67,156,239,67,67,98,108,11,203,67,204,185,67,67,244,245,202,67,164,94,65,67,60,
        216,202,67,52,179,62,67,99,109,80,53,229,66,44,53,67,67,98,240,114,229,66,204,32,67,67,176,218,223,66,20,223,61,67,80,13,213,66,200,6,52,67,108,16,135,196,66,140,247,36,67,108,208,221,193,66,156,164,35,67,98,48,103,192,66,56,234,34,67,80,104,191,66,152,
        131,34,67,80,167,191,66,156,192,34,67,98,112,12,193,66,64,26,36,67,112,137,228,66,60,85,67,67,176,172,228,66,204,84,67,67,98,240,195,228,66,204,84,67,67,112,1,229,66,60,70,67,67,80,53,229,66,44,53,67,67,99,109,144,177,237,66,76,148,66,67,98,144,189,238,
        66,180,132,66,67,208,152,239,66,44,99,66,67,208,152,239,66,180,73,66,67,98,208,152,239,66,244,14,66,67,80,63,200,66,204,198,27,67,144,180,199,66,148,122,27,67,98,80,99,199,66,4,78,27,67,144,64,199,66,192,24,28,67,144,15,199,66,56,58,31,67,108,112,209,
        198,66,96,50,35,67,108,176,37,200,66,136,84,36,67,98,208,224,200,66,32,244,36,67,112,53,208,66,128,155,43,67,16,112,216,66,216,29,51,67,98,176,170,224,66,36,160,58,67,112,236,231,66,196,61,65,67,112,144,232,66,100,209,65,67,98,144,171,233,66,92,208,66,
        67,208,199,233,66,180,220,66,67,144,194,234,66,60,199,66,67,98,176,83,235,66,188,186,66,67,208,165,236,66,236,163,66,67,176,177,237,66,92,148,66,67,99,109,120,196,101,67,44,119,66,67,98,56,184,101,67,188,66,66,67,104,138,101,67,148,102,65,67,216,94,101,
        67,252,141,64,67,98,56,51,101,67,92,181,63,67,136,8,101,67,52,253,62,67,232,255,100,67,188,244,62,67,98,56,243,100,67,68,232,62,67,8,74,91,67,68,201,64,67,136,249,89,67,180,25,65,67,98,24,197,89,67,44,38,65,67,40,154,89,67,76,75,65,67,40,154,89,67,12,
        108,65,67,98,40,154,89,67,132,156,65,67,216,18,90,67,68,168,65,67,72,32,92,67,228,170,65,67,98,56,204,94,67,108,174,65,67,136,193,97,67,124,254,65,67,8,242,99,67,188,126,66,67,98,200,200,101,67,108,234,66,67,120,223,101,67,12,234,66,67,120,196,101,67,
        60,119,66,67,99,109,68,7,207,67,156,217,61,67,98,60,233,207,67,148,171,59,67,228,60,209,67,132,199,56,67,100,22,210,67,108,47,55,67,98,116,120,210,67,92,119,54,67,44,191,210,67,188,234,53,67,132,179,210,67,220,246,53,67,98,220,167,210,67,236,2,54,67,
        116,85,209,67,124,76,55,67,132,195,207,67,20,211,56,67,98,148,49,206,67,172,89,58,67,148,229,204,67,172,158,59,67,188,225,204,67,76,165,59,67,98,228,218,204,67,36,177,59,67,204,18,205,67,148,157,64,67,180,41,205,67,60,249,65,67,108,164,53,205,67,108,
        174,66,67,108,116,202,205,67,116,19,65,67,98,84,28,206,67,100,49,64,67,228,170,206,67,196,189,62,67,68,7,207,67,156,217,61,67,99,109,144,39,248,66,36,217,65,67,98,16,10,250,66,100,193,65,67,240,242,253,66,244,173,65,67,200,107,0,67,244,173,65,67,98,40,
        222,1,67,244,173,65,67,40,13,3,67,172,161,65,67,40,13,3,67,196,146,65,67,98,40,13,3,67,172,122,65,67,144,14,224,66,104,249,19,67,144,231,223,66,224,226,19,67,98,208,208,223,66,148,213,19,67,48,20,202,66,176,36,25,67,16,193,201,66,0,76,25,67,98,240,153,
        201,66,76,94,25,67,208,213,210,66,12,153,34,67,176,69,222,66,84,206,45,67,98,240,88,242,66,188,122,65,67,112,24,243,66,124,46,66,67,176,229,243,66,204,25,66,67,98,112,90,244,66,236,13,66,67,240,68,246,66,228,240,65,67,144,39,248,66,28,217,65,67,99,109,
        140,185,191,67,164,104,63,67,98,180,215,191,67,76,34,63,67,236,209,193,67,60,178,55,67,180,224,193,67,132,79,55,67,98,116,231,193,67,156,34,55,67,68,136,191,67,72,153,49,67,76,122,191,67,56,181,49,67,98,60,118,191,67,84,189,49,67,60,125,191,67,96,108,
        50,67,220,137,191,67,28,58,51,67,98,84,203,191,67,36,102,55,67,20,116,191,67,244,246,59,67,236,148,190,67,220,10,64,67,98,108,92,190,67,4,19,65,67,108,42,190,67,204,253,65,67,196,37,190,67,132,20,66,67,98,252,27,190,67,28,68,66,67,212,136,191,67,76,218,
        63,67,148,185,191,67,164,104,63,67,99,109,68,177,198,67,68,130,65,67,98,212,162,198,67,92,101,65,67,28,138,198,67,108,100,65,67,44,104,198,67,148,127,65,67,98,12,56,198,67,12,166,65,67,84,57,198,67,172,168,65,67,4,126,198,67,60,171,65,67,98,44,176,198,
        67,76,173,65,67,68,192,198,67,52,160,65,67,68,177,198,67,60,130,65,67,99,109,24,236,114,67,220,227,63,67,98,24,236,114,67,20,165,63,67,56,18,115,67,100,236,62,67,216,64,115,67,116,73,62,67,98,152,183,115,67,28,170,60,67,200,182,115,67,212,166,60,67,136,
        229,114,67,76,200,60,67,98,200,132,114,67,172,215,60,67,104,40,114,67,140,236,60,67,88,24,114,67,124,246,60,67,98,88,8,114,67,116,0,61,67,216,33,114,67,68,208,61,67,104,81,114,67,100,196,62,67,98,184,161,114,67,228,97,64,67,8,236,114,67,4,236,64,67,8,
        236,114,67,212,227,63,67,99,109,156,80,180,67,12,238,63,67,98,204,15,182,67,76,6,63,67,204,132,183,67,220,24,60,67,140,249,183,67,124,147,56,67,98,124,33,184,67,84,95,55,67,244,38,184,67,140,16,53,67,172,4,184,67,84,224,51,67,98,212,186,183,67,144,80,
        49,67,188,227,182,67,228,240,46,67,60,191,181,67,152,117,45,67,98,212,146,180,67,248,239,43,67,52,227,178,67,112,131,43,67,156,131,177,67,28,101,44,67,98,148,241,175,67,40,103,45,67,140,180,174,67,32,225,47,67,140,51,174,67,48,5,51,67,98,180,6,174,67,
        152,28,52,67,172,252,173,67,36,173,52,67,204,252,173,67,72,24,54,67,98,204,252,173,67,60,38,56,67,172,44,174,67,36,143,57,67,36,170,174,67,156,57,59,67,98,236,244,174,67,180,55,60,67,52,179,175,67,52,219,61,67,196,32,176,67,124,115,62,67,98,124,122,176,
        67,52,240,62,67,60,50,177,67,164,152,63,67,148,175,177,67,20,225,63,67,98,68,85,178,67,204,64,64,67,100,164,179,67,68,71,64,67,140,80,180,67,244,237,63,67,99,109,60,163,178,67,180,199,59,67,98,140,204,177,67,164,119,59,67,228,28,177,67,164,148,58,67,
        244,168,176,67,108,57,57,67,98,244,229,175,67,96,241,54,67,20,12,176,67,84,252,51,67,156,5,177,67,60,9,50,67,98,252,127,178,67,128,20,47,67,60,7,181,67,184,52,48,67,108,192,181,67,112,36,52,67,98,36,234,181,67,108,7,53,67,204,238,181,67,108,244,54,67,
        92,201,181,67,28,213,55,67,98,52,131,181,67,12,122,57,67,28,190,180,67,244,8,59,67,44,246,179,67,212,134,59,67,98,180,154,179,67,100,192,59,67,60,234,178,67,52,226,59,67,60,163,178,67,180,199,59,67,99,109,108,18,180,67,196,100,58,67,98,92,136,180,67,
        244,238,57,67,156,245,180,67,36,18,57,67,108,55,181,67,172,20,56,67,98,180,124,181,67,148,9,55,67,76,125,181,67,116,45,53,67,164,56,181,67,212,22,52,67,98,52,122,180,67,172,17,49,67,252,135,178,67,92,96,48,67,148,92,177,67,44,183,50,67,98,148,26,176,
        67,44,59,53,67,188,160,176,67,148,141,57,67,60,83,178,67,44,169,58,67,98,172,208,178,67,20,251,58,67,236,154,179,67,36,220,58,67,108,18,180,67,204,100,58,67,99,109,156,29,197,67,32,40,54,67,98,52,84,197,67,252,199,53,67,220,128,197,67,156,108,53,67,220,
        128,197,67,24,93,53,67,98,220,128,197,67,132,77,53,67,148,43,197,67,76,117,52,67,100,195,196,67,116,124,51,67,98,44,91,196,67,160,131,50,67,196,7,196,67,0,181,49,67,12,10,196,67,88,177,49,67,98,84,12,196,67,156,173,49,67,236,161,196,67,212,224,49,67,
        132,86,197,67,4,35,50,67,98,20,11,198,67,56,101,50,67,68,179,198,67,28,156,50,67,52,204,198,67,252,156,50,67,98,36,240,198,67,16,159,50,67,252,34,199,67,124,25,50,67,212,191,199,67,52,33,48,67,108,4,134,200,67,208,163,45,67,108,132,151,200,67,196,89,
        46,67,98,52,192,200,67,20,1,48,67,60,172,200,67,20,242,47,67,212,74,201,67,132,224,46,67,98,252,152,201,67,172,89,46,67,116,220,201,67,144,230,45,67,196,224,201,67,172,224,45,67,98,228,236,201,67,12,208,45,67,140,93,201,67,0,53,38,67,84,80,201,67,120,
        54,38,67,98,228,73,201,67,120,54,38,67,76,149,200,67,8,96,40,67,252,190,199,67,16,3,43,67,98,172,232,198,67,20,166,45,67,172,48,198,67,100,206,47,67,36,38,198,67,100,206,47,67,98,148,27,198,67,100,206,47,67,220,234,196,67,168,100,47,67,252,128,195,67,
        120,227,46,67,98,20,23,194,67,64,98,46,67,228,230,192,67,44,254,45,67,4,221,192,67,12,5,46,67,98,180,205,192,67,168,15,46,67,20,189,193,67,192,65,48,67,252,11,196,67,96,126,53,67,98,172,127,196,67,216,132,54,67,20,135,196,67,204,161,54,67,52,112,196,
        67,40,6,55,67,108,132,87,196,67,140,114,55,67,108,244,136,196,67,180,36,55,67,98,28,164,196,67,236,249,54,67,4,231,196,67,68,136,54,67,156,29,197,67,32,40,54,67,99,109,228,124,225,67,148,76,54,67,98,140,248,225,67,40,223,53,67,60,98,226,67,208,7,53,67,
        252,189,226,67,16,190,51,67,98,148,51,227,67,188,23,50,67,164,51,227,67,84,251,49,67,172,191,226,67,92,158,48,67,98,172,27,226,67,244,176,46,67,252,162,225,67,240,239,43,67,252,162,225,67,44,31,42,67,98,252,162,225,67,144,113,41,67,180,157,225,67,180,
        94,41,67,76,96,225,67,240,51,41,67,98,244,241,224,67,8,231,40,67,20,124,223,67,116,10,41,67,28,208,222,67,36,114,41,67,98,220,164,222,67,52,140,41,67,76,54,222,67,156,225,43,67,204,7,222,67,16,172,45,67,98,108,212,221,67,128,167,47,67,252,228,221,67,
        140,149,50,67,236,41,222,67,248,177,51,67,98,204,129,222,67,128,28,53,67,180,45,223,67,208,54,54,67,196,240,223,67,204,156,54,67,98,60,83,224,67,72,208,54,67,52,20,225,67,72,169,54,67,236,124,225,67,148,76,54,67,99,109,0,13,191,65,156,242,51,67,98,64,
        34,198,65,104,84,51,67,0,200,210,65,184,174,49,67,128,12,227,65,120,66,47,67,98,64,67,234,65,112,47,46,67,128,130,244,65,156,168,44,67,128,210,249,65,248,221,43,67,98,64,72,6,66,16,19,41,67,64,57,10,66,244,155,39,67,96,69,10,66,92,230,38,67,98,192,77,
        10,66,76,141,38,67,0,230,0,66,140,150,40,67,128,249,241,65,64,165,42,67,98,0,47,226,65,236,178,44,67,64,161,207,65,232,130,47,67,128,2,191,65,24,95,50,67,98,128,250,176,65,44,201,52,67,64,110,177,65,156,173,52,67,192,234,181,65,148,137,52,67,98,128,245,
        183,65,40,121,52,67,128,17,188,65,68,53,52,67,0,13,191,65,164,242,51,67,99,109,100,33,233,67,92,29,50,67,98,60,166,234,67,160,147,49,67,116,78,236,67,44,220,47,67,28,191,237,67,80,85,45,67,98,212,26,239,67,52,243,42,67,196,78,240,67,24,42,39,67,180,207,
        240,67,56,176,35,67,98,228,120,241,67,144,32,31,67,220,53,241,67,80,18,27,67,172,13,240,67,124,242,23,67,98,140,123,238,67,168,180,19,67,132,221,235,67,136,247,16,67,28,162,232,67,216,46,16,67,98,220,174,231,67,212,243,15,67,60,41,229,67,124,21,16,67,
        60,62,228,67,112,105,16,67,108,244,169,227,67,108,158,16,67,108,196,164,227,67,248,47,18,67,98,140,148,227,67,156,12,23,67,52,151,226,67,36,97,28,67,156,130,224,67,72,16,35,67,98,116,25,224,67,36,98,36,67,228,159,223,67,0,246,37,67,108,116,223,67,180,
        145,38,67,108,116,37,223,67,216,172,39,67,108,44,10,224,67,64,150,39,67,98,252,135,224,67,196,137,39,67,172,22,225,67,72,146,39,67,60,71,225,67,244,168,39,67,108,148,159,225,67,76,210,39,67,108,220,183,225,67,156,185,38,67,98,44,24,226,67,68,97,34,67,
        204,83,227,67,96,9,31,67,108,60,229,67,152,47,29,67,98,172,209,230,67,176,166,27,67,20,117,232,67,104,142,27,67,36,207,233,67,200,235,28,67,98,132,89,235,67,236,121,30,67,68,43,236,67,32,166,32,67,68,103,236,67,172,204,35,67,98,252,176,236,67,32,172,
        39,67,84,133,235,67,40,44,43,67,252,239,233,67,40,44,43,67,98,84,133,233,67,40,44,43,67,180,155,233,67,140,228,42,67,156,32,234,67,204,144,42,67,98,140,189,234,67,220,45,42,67,84,100,235,67,196,203,40,67,156,155,235,67,48,108,39,67,98,4,22,236,67,16,
        98,36,67,180,36,235,67,52,148,32,67,60,131,233,67,152,247,30,67,98,140,239,232,67,160,101,30,67,28,212,232,67,224,88,30,67,124,45,232,67,224,88,30,67,98,36,109,231,67,224,88,30,67,180,240,230,67,228,162,30,67,52,208,229,67,220,192,31,67,98,148,115,228,
        67,108,26,33,67,68,109,227,67,236,153,36,67,244,80,227,67,204,70,40,67,98,132,69,227,67,4,196,41,67,132,69,227,67,40,196,41,67,228,153,227,67,120,3,43,67,98,84,200,227,67,32,179,43,67,76,0,228,67,156,206,44,67,68,22,228,67,96,121,45,67,98,68,59,228,67,
        248,152,46,67,20,72,228,67,80,194,46,67,172,195,228,67,156,169,47,67,98,164,87,229,67,120,190,48,67,172,49,230,67,184,174,49,67,44,234,230,67,48,8,50,67,98,4,126,231,67,224,79,50,67,124,120,232,67,56,89,50,67,108,33,233,67,104,29,50,67,99,109,12,21,214,
        67,132,168,48,67,98,196,221,215,67,172,36,46,67,68,59,217,67,116,141,44,67,156,73,219,67,232,151,42,67,98,204,244,219,67,200,244,41,67,4,129,220,67,100,87,41,67,60,129,220,67,32,58,41,67,98,100,130,220,67,32,174,40,67,204,131,221,67,20,118,36,67,228,
        98,222,67,172,85,33,67,98,84,73,224,67,40,132,26,67,44,228,224,67,196,184,23,67,132,45,225,67,144,111,20,67,98,12,89,225,67,76,124,18,67,36,89,225,67,144,158,17,67,132,45,225,67,208,191,17,67,98,172,27,225,67,140,205,17,67,60,200,224,67,40,2,18,67,44,
        116,224,67,208,52,18,67,98,180,12,222,67,124,167,19,67,84,253,218,67,188,170,22,67,140,38,216,67,132,97,26,67,98,188,85,213,67,128,16,30,67,68,156,210,67,216,115,34,67,236,50,206,67,164,104,42,67,98,236,135,204,67,192,106,45,67,12,53,204,67,36,18,46,
        67,76,86,204,67,116,43,46,67,98,108,108,204,67,76,60,46,67,108,56,205,67,172,143,46,67,156,27,206,67,200,228,46,67,98,204,254,206,67,228,57,47,67,244,228,208,67,180,11,48,67,236,83,210,67,20,183,48,67,98,236,194,211,67,124,98,49,67,44,252,212,67,40,240,
        49,67,12,12,213,67,244,241,49,67,98,228,27,213,67,12,244,49,67,12,147,213,67,132,95,49,67,212,20,214,67,144,168,48,67,99,109,160,142,16,66,160,128,31,67,98,32,249,23,66,56,71,30,67,224,159,30,66,20,18,29,67,160,86,31,66,168,209,28,67,98,96,146,34,66,
        192,173,27,67,128,53,36,66,4,77,25,67,128,200,34,66,140,208,23,67,98,32,206,33,66,156,203,22,67,192,234,30,66,164,228,21,67,32,37,28,66,172,189,21,67,98,128,32,24,66,44,133,21,67,192,154,15,66,248,34,22,67,96,91,10,66,0,7,23,67,98,96,11,6,66,100,194,
        23,67,0,158,243,65,104,47,26,67,0,138,243,65,220,80,26,67,98,64,121,243,65,216,95,26,67,0,24,255,65,40,200,26,67,64,165,6,66,196,56,27,67,98,192,190,13,66,100,169,27,67,192,140,19,66,140,15,28,67,128,139,19,66,212,27,28,67,98,128,139,19,66,24,40,28,67,
        160,195,11,66,224,22,29,67,160,67,2,66,128,46,30,67,108,128,252,225,65,224,42,32,67,108,0,211,226,65,40,168,33,67,98,0,73,227,65,220,121,34,67,128,1,228,65,60,136,35,67,128,109,228,65,248,0,36,67,108,0,50,229,65,132,220,36,67,108,64,172,245,65,120,75,
        35,67,98,64,188,254,65,228,110,34,67,128,36,9,66,0,186,32,67,192,142,16,66,148,128,31,67,99,109,176,114,208,66,148,3,22,67,108,16,60,220,66,128,25,19,67,108,240,116,212,66,164,254,18,67,98,208,45,208,66,224,239,18,67,112,251,202,66,64,225,18,67,208,232,
        200,66,36,222,18,67,108,240,35,197,66,136,216,18,67,108,80,98,197,66,228,64,14,67,108,208,160,197,66,68,169,9,67,108,80,186,190,66,216,62,14,67,98,16,2,184,66,172,181,18,67,16,204,183,66,112,212,18,67,48,171,182,66,56,213,18,67,98,48,8,182,66,56,213,
        18,67,112,252,176,66,108,151,18,67,208,116,171,66,236,74,18,67,108,240,102,161,66,208,191,17,67,108,240,177,162,66,200,86,18,67,98,240,103,163,66,204,169,18,67,144,160,166,66,220,1,20,67,80,218,169,66,92,83,21,67,98,48,20,173,66,216,164,22,67,240,183,
        175,66,220,195,23,67,240,183,175,66,48,209,23,67,98,240,183,175,66,124,222,23,67,240,76,172,66,120,234,25,67,144,31,168,66,156,93,28,67,98,48,242,163,66,188,208,30,67,240,145,160,66,36,215,32,67,208,158,160,66,164,221,32,67,98,176,171,160,66,20,228,32,
        67,144,161,165,66,96,165,31,67,80,164,171,66,76,25,30,67,98,48,167,177,66,60,141,28,67,208,181,182,66,48,73,27,67,112,225,182,66,48,73,27,67,98,176,70,183,66,48,73,27,67,112,136,194,66,24,4,33,67,112,105,195,66,52,170,33,67,108,208,18,196,66,88,39,34,
        67,108,48,94,196,66,136,138,29,67,108,112,169,196,66,188,237,24,67,108,208,114,208,66,168,3,22,67,99,109,48,25,169,66,204,102,24,67,98,48,25,169,66,196,76,24,67,112,210,167,66,220,149,23,67,48,67,166,66,84,208,22,67,108,16,109,163,66,40,105,21,67,108,
        240,165,165,66,200,70,23,67,98,144,198,167,66,228,15,25,67,176,229,167,66,84,33,25,67,48,124,168,66,64,221,24,67,98,144,210,168,66,36,182,24,67,80,25,169,66,216,128,24,67,80,25,169,66,204,102,24,67,99,109,208,136,204,66,108,82,16,67,98,240,49,204,66,
        100,239,15,67,16,62,203,66,64,233,14,67,16,107,202,66,224,11,14,67,108,48,235,200,66,112,121,12,67,108,80,229,200,66,60,167,14,67,98,48,225,200,66,32,64,16,67,208,251,200,66,196,218,16,67,80,73,201,66,112,234,16,67,98,144,131,201,66,20,246,16,67,48,122,
        202,66,76,1,17,67,16,109,203,66,32,3,17,67,108,240,38,205,66,116,6,17,67,108,240,136,204,66,108,82,16,67,99,109,252,186,171,67,172,208,61,67,108,140,81,171,67,180,159,61,67,108,84,248,170,67,4,190,59,67,108,28,159,170,67,76,220,57,67,108,100,247,170,
        67,124,242,56,67,108,172,79,171,67,180,8,56,67,108,220,106,171,67,116,23,57,67,98,140,144,171,67,36,142,58,67,220,190,171,67,4,157,59,67,132,18,172,67,220,235,60,67,98,156,95,172,67,140,32,62,67,188,96,172,67,180,29,62,67,4,187,171,67,172,208,61,67,99,
        109,200,185,3,67,164,4,59,67,98,200,110,3,67,252,137,57,67,88,40,3,67,140,45,56,67,40,29,3,67,84,254,55,67,98,56,10,3,67,140,174,55,67,88,60,3,67,84,158,55,67,184,220,5,67,84,26,55,67,98,216,106,7,67,40,204,54,67,136,183,8,67,124,147,54,67,24,192,8,67,
        96,156,54,67,98,232,209,8,67,20,175,54,67,184,207,9,67,92,155,59,67,72,224,9,67,196,51,60,67,98,24,235,9,67,140,150,60,67,184,214,9,67,236,157,60,67,136,69,7,67,124,38,61,67,98,184,208,5,67,244,115,61,67,152,138,4,67,188,179,61,67,200,112,4,67,52,180,
        61,67,98,232,85,4,67,52,180,61,67,216,7,4,67,212,143,60,67,168,185,3,67,164,4,59,67,99,109,152,208,26,67,188,108,60,67,98,232,195,26,67,252,67,60,67,104,63,26,67,220,195,57,67,24,170,25,67,80,222,54,67,98,120,240,24,67,100,68,51,67,184,169,24,67,24,149,
        49,67,152,202,24,67,124,138,49,67,98,24,48,25,67,184,105,49,67,232,10,35,67,188,120,47,67,40,21,35,67,124,131,47,67,98,120,41,35,67,28,153,47,67,8,85,37,67,28,152,58,67,88,71,37,67,36,165,58,67,98,56,57,37,67,164,178,58,67,72,56,27,67,44,183,60,67,120,
        4,27,67,4,183,60,67,98,184,244,26,67,244,182,60,67,56,221,26,67,132,149,60,67,136,208,26,67,196,108,60,67,99,109,216,224,50,67,204,67,57,67,98,216,224,50,67,12,48,57,67,40,84,50,67,76,111,54,67,40,168,49,67,136,37,51,67,98,216,148,48,67,152,225,45,67,
        184,119,48,67,168,40,45,67,152,180,48,67,12,23,45,67,98,216,130,49,67,76,219,44,67,24,217,59,67,232,209,42,67,8,233,59,67,216,225,42,67,98,184,254,59,67,132,247,42,67,24,112,62,67,124,22,55,67,200,94,62,67,124,22,55,67,98,136,88,62,67,124,22,55,67,232,
        199,59,67,236,155,55,67,152,171,56,67,4,63,56,67,98,88,77,50,67,252,140,57,67,216,224,50,67,4,115,57,67,216,224,50,67,204,67,57,67,99,109,136,52,75,67,12,141,53,67,98,136,52,75,67,100,134,53,67,136,165,74,67,24,196,50,67,200,246,73,67,128,107,47,67,98,
        8,72,73,67,236,18,44,67,8,185,72,67,180,61,41,67,8,185,72,67,232,31,41,67,98,8,185,72,67,164,250,40,67,40,144,74,67,12,139,40,67,40,160,78,67,240,185,39,67,98,72,223,81,67,212,18,39,67,104,138,84,67,216,142,38,67,40,142,84,67,160,148,38,67,98,152,159,
        84,67,44,175,38,67,184,30,87,67,236,43,51,67,168,20,87,67,0,54,51,67,98,120,6,87,67,36,68,51,67,216,146,75,67,44,153,53,67,248,91,75,67,44,153,53,67,98,72,70,75,67,44,153,53,67,136,52,75,67,196,147,53,67,136,52,75,67,32,141,53,67,99,109,72,222,99,67,
        136,24,48,67,98,120,2,99,67,124,31,44,67,152,186,97,67,72,121,37,67,152,206,97,67,36,101,37,67,98,72,220,97,67,108,87,37,67,8,80,100,67,20,206,36,67,152,65,103,67,240,51,36,67,108,184,155,108,67,176,27,35,67,108,136,218,108,67,240,51,36,67,98,40,126,
        109,67,172,13,39,67,72,247,110,67,28,176,46,67,40,228,110,67,60,195,46,67,98,232,205,110,67,120,217,46,67,232,151,100,67,0,246,48,67,200,63,100,67,156,246,48,67,98,152,36,100,67,204,246,48,67,184,248,99,67,216,146,48,67,72,222,99,67,128,24,48,67,99,109,
        152,37,13,67,244,105,48,67,98,152,37,13,67,68,98,48,67,40,188,12,67,60,92,46,67,88,59,12,67,188,234,43,67,98,24,177,11,67,40,75,41,67,168,97,11,67,68,111,39,67,104,121,11,67,152,96,39,67,98,184,194,11,67,60,51,39,67,24,198,19,67,240,169,37,67,88,224,
        19,67,52,196,37,67,98,88,4,20,67,56,232,37,67,120,196,21,67,16,176,46,67,120,172,21,67,32,200,46,67,98,120,150,21,67,20,222,46,67,232,178,13,67,228,119,48,67,168,92,13,67,228,119,48,67,98,88,62,13,67,228,119,48,67,152,37,13,67,164,113,48,67,152,37,13,
        67,244,105,48,67,99,109,56,212,35,67,4,127,40,67,98,248,66,35,67,168,168,37,67,8,201,34,67,52,39,35,67,40,197,34,67,140,237,34,67,98,88,190,34,67,104,133,34,67,72,199,34,67,240,130,34,67,72,221,39,67,208,120,33,67,98,88,174,42,67,128,229,32,67,136,5,
        45,67,40,118,32,67,200,16,45,67,108,129,32,67,98,168,37,45,67,76,150,32,67,136,103,47,67,28,143,43,67,152,89,47,67,244,155,43,67,98,24,76,47,67,112,168,43,67,120,59,37,67,216,167,45,67,232,10,37,67,184,167,45,67,98,136,239,36,67,172,167,45,67,248,110,
        36,67,112,133,43,67,40,212,35,67,252,126,40,67,99,109,248,90,124,67,208,13,45,67,98,248,90,124,67,8,255,44,67,136,211,123,67,120,88,42,67,8,46,123,67,192,41,39,67,98,152,136,122,67,4,251,35,67,88,17,122,67,192,81,33,67,72,37,122,67,220,63,33,67,98,232,
        72,122,67,212,31,33,67,52,147,130,67,80,225,30,67,52,154,130,67,28,247,30,67,98,92,163,130,67,104,19,31,67,36,207,131,67,152,226,42,67,156,201,131,67,24,234,42,67,98,52,193,131,67,80,245,42,67,136,160,124,67,144,40,45,67,120,121,124,67,144,40,45,67,98,
        152,104,124,67,144,40,45,67,8,91,124,67,128,28,45,67,8,91,124,67,216,13,45,67,99,109,84,17,165,67,36,241,43,67,98,84,17,165,67,60,183,43,67,164,12,165,67,116,110,43,67,236,6,165,67,104,79,43,67,98,44,0,165,67,188,42,43,67,60,38,165,67,228,242,42,67,180,
        115,165,67,228,175,42,67,98,196,184,165,67,36,116,42,67,116,23,166,67,152,239,41,67,236,84,166,67,180,116,41,67,98,196,185,167,67,16,171,38,67,172,128,167,67,104,8,34,67,212,222,165,67,184,221,31,67,98,36,70,164,67,44,191,29,67,108,45,162,67,232,224,
        30,67,116,77,161,67,248,84,34,67,98,172,18,161,67,20,61,35,67,236,13,161,67,152,113,35,67,236,13,161,67,180,19,37,67,98,236,13,161,67,64,12,38,67,220,9,161,67,100,207,38,67,196,4,161,67,92,197,38,67,98,196,255,160,67,100,187,38,67,44,226,160,67,228,227,
        38,67,20,195,160,67,144,31,39,67,108,116,138,160,67,4,140,39,67,108,36,114,160,67,4,154,38,67,98,156,25,160,67,44,40,35,67,84,235,160,67,236,206,31,67,20,121,162,67,208,80,30,67,98,204,251,163,67,72,221,28,67,108,166,165,67,100,112,29,67,180,209,166,
        67,120,208,31,67,98,180,141,168,67,152,86,35,67,140,19,168,67,24,116,41,67,20,229,165,67,100,169,43,67,98,60,44,165,67,128,100,44,67,60,17,165,67,160,109,44,67,60,17,165,67,36,241,43,67,99,109,40,205,60,67,212,154,42,67,98,40,205,60,67,188,152,42,67,
        136,55,60,67,24,190,39,67,152,130,59,67,32,66,36,67,98,168,66,58,67,140,24,30,67,24,60,58,67,204,235,29,67,168,144,58,67,44,216,29,67,98,248,84,59,67,140,170,29,67,104,92,69,67,0,164,27,67,248,186,69,67,224,150,27,67,98,232,32,70,67,192,136,27,67,168,
        34,70,67,48,143,27,67,40,104,71,67,144,210,33,67,98,232,27,72,67,4,72,37,67,216,168,72,67,68,35,40,67,88,161,72,67,152,43,40,67,98,40,150,72,67,20,56,40,67,40,210,60,67,252,163,42,67,40,205,60,67,216,154,42,67,99,109,100,167,164,67,64,7,40,67,98,68,138,
        164,67,136,244,39,67,244,128,164,67,112,227,39,67,180,146,164,67,24,225,39,67,98,180,195,164,67,168,218,39,67,116,77,165,67,160,231,38,67,132,115,165,67,108,84,38,67,98,20,134,165,67,152,12,38,67,76,149,165,67,20,121,37,67,76,149,165,67,172,12,37,67,
        98,76,149,165,67,64,158,34,67,180,47,164,67,52,53,33,67,236,64,163,67,124,178,34,67,98,52,145,162,67,28,203,35,67,244,117,162,67,72,205,37,67,116,6,163,67,228,1,39,67,108,172,52,163,67,168,100,39,67,108,36,245,162,67,240,70,39,67,98,52,169,162,67,108,
        35,39,67,92,135,162,67,68,214,38,67,92,107,162,67,204,12,38,67,98,140,47,162,67,44,94,36,67,228,151,162,67,100,146,34,67,156,96,163,67,212,211,33,67,98,52,188,163,67,236,124,33,67,92,115,164,67,220,119,33,67,20,212,164,67,108,201,33,67,98,68,255,165,
        67,52,198,34,67,212,62,166,67,40,248,37,67,204,72,165,67,36,168,39,67,98,252,244,164,67,72,59,40,67,252,246,164,67,28,58,40,67,92,167,164,67,24,7,40,67,99,109,152,41,85,67,172,195,37,67,98,216,188,83,67,188,15,31,67,152,206,82,67,4,18,26,67,216,246,82,
        67,44,249,25,67,98,200,58,83,67,44,207,25,67,88,99,94,67,112,147,23,67,248,122,94,67,20,171,23,67,98,120,148,94,67,152,196,23,67,104,22,97,67,8,20,36,67,184,4,97,67,0,33,36,67,98,120,253,96,67,104,38,36,67,72,111,94,67,64,172,36,67,24,87,91,67,148,74,
        37,67,98,216,62,88,67,224,232,37,67,184,160,85,67,212,114,38,67,216,133,85,67,32,125,38,67,98,72,104,85,67,88,136,38,67,232,67,85,67,84,63,38,67,152,41,85,67,172,195,37,67,99,109,36,85,139,67,228,236,36,67,98,164,71,139,67,240,102,36,67,172,33,139,67,
        236,247,34,67,196,0,139,67,96,189,33,67,98,228,223,138,67,212,130,32,67,52,200,138,67,220,122,31,67,52,204,138,67,208,114,31,67,98,36,208,138,67,184,106,31,67,204,127,139,67,76,28,31,67,132,82,140,67,108,196,30,67,98,148,208,141,67,28,37,30,67,164,209,
        141,67,236,36,30,67,44,220,141,67,80,126,30,67,98,196,0,142,67,4,180,31,67,52,121,142,67,52,158,36,67,92,115,142,67,220,169,36,67,98,252,107,142,67,160,184,36,67,52,154,139,67,120,224,37,67,204,125,139,67,120,224,37,67,98,244,116,139,67,120,224,37,67,
        164,98,139,67,220,114,37,67,28,85,139,67,220,236,36,67,99,109,152,2,1,67,208,118,33,67,98,24,200,0,67,152,170,32,67,216,140,0,67,12,55,31,67,40,159,0,67,76,7,31,67,98,72,178,0,67,32,213,30,67,136,254,2,67,68,81,30,67,8,36,3,67,180,118,30,67,98,24,54,
        3,67,204,136,30,67,184,182,3,67,136,211,32,67,184,182,3,67,172,19,33,67,98,184,182,3,67,220,39,33,67,8,73,3,67,72,78,33,67,24,195,2,67,48,105,33,67,98,40,61,2,67,24,132,33,67,88,166,1,67,168,163,33,67,40,116,1,67,88,175,33,67,98,88,52,1,67,28,190,33,
        67,40,18,1,67,12,173,33,67,152,2,1,67,208,118,33,67,99,109,184,117,110,67,124,53,32,67,98,232,245,109,67,124,228,29,67,56,162,108,67,152,64,23,67,56,162,108,67,60,18,23,67,98,56,162,108,67,88,239,22,67,120,23,110,67,8,147,22,67,248,246,112,67,8,0,22,
        67,98,200,88,115,67,32,134,21,67,40,123,117,67,64,31,21,67,40,181,117,67,100,27,21,67,98,248,29,118,67,136,20,21,67,24,32,118,67,144,27,21,67,184,38,119,67,8,25,26,67,98,120,220,119,67,216,140,29,67,184,31,120,67,116,34,31,67,104,254,119,67,32,45,31,
        67,98,136,205,119,67,180,60,31,67,232,207,110,67,224,19,33,67,120,180,110,67,224,19,33,67,98,88,172,110,67,224,19,33,67,24,144,110,67,204,175,32,67,200,117,110,67,120,53,32,67,99,109,40,126,23,67,152,180,32,67,98,232,88,23,67,96,69,32,67,56,229,21,67,
        44,197,24,67,72,242,21,67,12,184,24,67,98,104,9,22,67,236,160,24,67,120,140,29,67,120,28,23,67,24,153,29,67,92,44,23,67,98,200,181,29,67,76,80,23,67,104,57,31,67,220,69,31,67,24,39,31,67,56,88,31,67,98,24,23,31,67,60,104,31,67,40,219,23,67,140,233,32,
        67,104,158,23,67,140,233,32,67,98,72,150,23,67,140,233,32,67,232,135,23,67,180,209,32,67,40,126,23,67,148,180,32,67,99,109,232,120,47,67,72,138,29,67,98,72,60,47,67,52,232,28,67,232,154,45,67,228,63,20,67,200,180,45,67,236,41,20,67,98,24,235,45,67,216,
        251,19,67,56,95,54,67,32,80,18,67,184,124,54,67,160,109,18,67,98,248,156,54,67,224,141,18,67,104,135,56,67,24,208,27,67,56,114,56,67,72,229,27,67,98,120,106,56,67,248,236,27,67,88,102,54,67,176,89,28,67,88,247,51,67,168,214,28,67,98,88,239,48,67,84,114,
        29,67,8,133,47,67,228,170,29,67,216,120,47,67,80,138,29,67,99,109,76,248,131,67,116,129,27,67,98,76,248,131,67,32,110,27,67,116,204,131,67,236,180,25,67,204,150,131,67,248,172,23,67,98,36,97,131,67,8,165,21,67,76,57,131,67,140,243,19,67,68,62,131,67,
        172,233,19,67,98,68,79,131,67,172,199,19,67,28,203,134,67,24,106,18,67,172,210,134,67,124,130,18,67,98,140,226,134,67,232,181,18,67,172,156,135,67,148,3,26,67,172,147,135,67,148,21,26,67,98,124,142,135,67,252,31,26,67,204,214,134,67,244,114,26,67,132,
        251,133,67,240,205,26,67,98,52,32,133,67,244,40,27,67,148,82,132,67,120,126,27,67,132,50,132,67,0,140,27,67,98,124,18,132,67,128,153,27,67,76,248,131,67,184,148,27,67,84,248,131,67,100,129,27,67,99,109,104,185,71,67,204,22,26,67,98,104,185,71,67,140,
        16,26,67,248,63,71,67,188,192,23,67,40,173,70,67,176,243,20,67,98,72,167,69,67,208,244,15,67,232,163,69,67,156,219,15,67,120,250,69,67,156,199,15,67,98,40,6,71,67,200,137,15,67,104,63,79,67,40,230,13,67,248,67,79,67,192,237,13,67,98,248,89,79,67,76,18,
        14,67,120,74,81,67,108,17,24,67,8,62,81,67,232,29,24,67,98,72,53,81,67,164,38,24,67,152,56,79,67,140,148,24,67,184,211,76,67,244,17,25,67,98,232,110,74,67,92,143,25,67,136,78,72,67,236,255,25,67,24,26,72,67,20,12,26,67,98,168,229,71,67,88,24,26,67,40,
        186,71,67,36,29,26,67,104,185,71,67,228,22,26,67,99,109,12,218,144,67,148,94,21,67,98,172,200,144,67,80,34,21,67,164,109,144,67,120,95,17,67,132,119,144,67,96,76,17,67,98,172,137,144,67,44,41,17,67,124,91,146,67,64,116,16,67,68,103,146,67,208,139,16,
        67,98,252,115,146,67,60,165,16,67,164,224,146,67,104,179,20,67,236,215,146,67,12,192,20,67,98,212,208,146,67,56,202,20,67,228,4,145,67,108,126,21,67,220,241,144,67,96,126,21,67,98,196,233,144,67,92,126,21,67,28,223,144,67,8,112,21,67,12,218,144,67,148,
        94,21,67,99,109,8,188,95,67,228,251,16,67,98,232,65,95,67,68,152,14,67,24,219,94,67,196,146,12,67,88,215,94,67,236,125,12,67,98,200,204,94,67,204,65,12,67,200,46,103,67,168,145,10,67,216,67,103,67,176,203,10,67,98,248,97,103,67,140,30,11,67,136,25,105,
        67,84,172,19,67,8,12,105,67,84,172,19,67,98,248,4,105,67,84,172,19,67,216,41,103,67,168,11,20,67,248,235,100,67,40,128,20,67,98,40,174,98,67,168,244,20,67,152,202,96,67,252,83,21,67,72,185,96,67,252,83,21,67,98,8,168,96,67,252,83,21,67,8,54,96,67,140,
        95,19,67,248,187,95,67,228,251,16,67,99,109,72,68,11,67,64,15,18,67,98,216,56,11,67,124,230,17,67,8,35,11,67,100,130,17,67,232,19,11,67,216,48,17,67,98,184,4,11,67,72,223,16,67,216,239,10,67,200,125,16,67,72,229,10,67,36,88,16,67,98,88,214,10,67,4,34,
        16,67,152,8,11,67,4,9,16,67,152,213,11,67,176,224,15,67,98,72,100,12,67,160,196,15,67,200,226,12,67,128,183,15,67,200,238,12,67,128,195,15,67,98,216,250,12,67,144,207,15,67,248,28,13,67,36,78,16,67,200,58,13,67,236,220,16,67,98,168,118,13,67,28,252,17,
        67,72,132,13,67,148,232,17,67,168,81,12,67,224,44,18,67,98,168,87,11,67,136,100,18,67,120,92,11,67,12,101,18,67,72,68,11,67,64,15,18,67,99,109,232,137,121,67,16,63,15,67,98,216,110,121,67,20,185,14,67,200,25,121,67,200,29,13,67,248,204,120,67,24,173,
        11,67,98,104,107,120,67,44,217,9,67,200,79,120,67,232,9,9,67,120,113,120,67,208,254,8,67,98,24,206,120,67,92,224,8,67,104,151,126,67,180,193,7,67,88,215,126,67,224,193,7,67,98,104,9,127,67,0,194,7,67,200,63,127,67,244,138,8,67,120,202,127,67,172,68,11,
        67,98,92,22,128,67,236,50,13,67,132,55,128,67,148,211,14,67,236,46,128,67,140,226,14,67,98,180,31,128,67,24,253,14,67,152,80,122,67,184,50,16,67,24,237,121,67,172,50,16,67,98,152,209,121,67,164,50,16,67,248,164,121,67,8,197,15,67,248,137,121,67,8,63,
        15,67,99,109,120,231,34,67,108,121,15,67,98,120,189,34,67,144,248,14,67,232,13,34,67,104,59,11,67,120,29,34,67,224,43,11,67,98,8,62,34,67,68,11,11,67,24,32,38,67,48,87,10,67,168,54,38,67,196,109,10,67,98,232,82,38,67,0,138,10,67,136,44,39,67,4,184,14,
        67,184,25,39,67,204,201,14,67,98,8,18,39,67,24,209,14,67,168,32,38,67,88,6,15,67,88,1,37,67,32,64,15,67,98,24,147,35,67,208,137,15,67,120,242,34,67,240,154,15,67,120,231,34,67,112,121,15,67,99,109,232,226,57,67,32,149,9,67,98,88,73,57,67,108,166,6,67,
        136,61,57,67,176,68,6,67,104,122,57,67,44,51,6,67,98,248,11,58,67,68,9,6,67,24,129,63,67,88,240,4,67,24,137,63,67,92,251,4,67,98,216,141,63,67,208,1,5,67,136,221,63,67,108,134,6,67,72,58,64,67,0,91,8,67,108,232,226,64,67,4,175,11,67,108,104,250,61,67,
        120,73,12,67,98,232,96,60,67,100,158,12,67,184,244,58,67,192,227,12,67,248,208,58,67,160,227,12,67,98,152,158,58,67,160,227,12,67,24,105,58,67,44,37,12,67,232,226,57,67,32,149,9,67,99,109,104,18,83,67,72,139,8,67,98,168,226,82,67,176,245,7,67,232,207,
        81,67,104,107,2,67,168,223,81,67,168,91,2,67,98,8,1,82,67,72,58,2,67,232,198,87,67,244,19,1,67,56,221,87,67,68,42,1,67,98,88,240,87,67,104,61,1,67,248,66,89,67,164,151,7,67,200,51,89,67,164,151,7,67,98,184,48,89,67,164,151,7,67,248,238,87,67,96,218,7,
        67,24,105,86,67,236,43,8,67,98,200,80,83,67,164,209,8,67,56,42,83,67,244,213,8,67,104,18,83,67,64,139,8,67,99,109,72,17,20,67,196,107,2,67,98,248,199,19,67,176,246,0,67,168,177,19,67,52,49,0,67,216,206,19,67,244,30,0,67,98,40,8,20,67,56,246,255,66,152,
        187,23,67,216,129,254,66,40,204,23,67,120,172,254,66,98,232,227,23,67,200,233,254,66,184,188,24,67,108,171,3,67,8,174,24,67,16,186,3,67,98,56,163,24,67,224,196,3,67,136,173,20,67,32,157,4,67,232,133,20,67,32,157,4,67,98,88,130,20,67,32,157,4,67,232,77,
        20,67,136,160,3,67,72,17,20,67,196,107,2,67,99,109,56,162,108,67,76,10,3,67,98,56,162,108,67,76,5,3,67,120,122,108,67,100,62,2,67,232,73,108,67,52,80,1,67,98,88,25,108,67,16,98,0,67,216,251,107,67,216,41,255,66,104,8,108,67,200,16,255,66,98,232,37,108,
        67,168,213,254,66,120,38,111,67,248,153,253,66,8,58,111,67,240,192,253,66,98,72,79,111,67,136,235,253,66,136,239,111,67,204,212,1,67,168,240,111,67,52,38,2,67,98,168,240,111,67,208,90,2,67,8,145,111,67,164,125,2,67,232,73,110,67,220,190,2,67,98,232,96,
        109,67,88,237,2,67,56,162,108,67,68,15,3,67,56,162,108,67,68,10,3,67,99,109,104,93,44,67,4,46,1,67,98,120,252,43,67,240,169,255,66,56,62,43,67,8,70,247,66,24,93,43,67,192,14,247,66,98,216,129,43,67,0,205,246,66,184,190,48,67,120,160,244,66,88,210,48,
        67,200,202,244,66,98,232,243,48,67,248,18,245,66,232,0,50,67,144,63,0,67,232,233,49,67,248,84,0,67,98,8,209,49,67,20,108,0,67,232,219,44,67,60,122,1,67,40,143,44,67,184,120,1,67,98,88,127,44,67,184,120,1,67,232,104,44,67,208,86,1,67,104,93,44,67,4,46,
        1,67,99,109,216,149,68,67,216,211,251,66,98,216,149,68,67,200,204,251,66,168,73,68,67,240,209,248,66,136,236,67,67,240,52,245,66,98,88,143,67,67,0,152,241,66,136,77,67,67,96,142,238,66,72,90,67,67,232,116,238,66,98,152,117,67,67,40,62,238,66,88,113,73,
        67,176,198,235,66,72,131,73,67,176,234,235,66,98,24,156,73,67,96,28,236,66,232,231,74,67,88,14,249,66,88,215,74,67,96,47,249,66,98,184,198,74,67,160,80,249,66,200,149,68,67,8,238,251,66,200,149,68,67,216,211,251,66,99,109,88,123,93,67,240,150,241,66,
        98,120,16,93,67,160,49,238,66,88,117,92,67,120,151,231,66,168,140,92,67,48,110,231,66,98,72,188,92,67,16,26,231,66,104,97,97,67,232,94,229,66,88,125,97,67,232,150,229,66,98,24,155,97,67,88,210,229,66,120,133,98,67,56,152,238,66,40,136,98,67,152,147,239,
        66,98,56,138,98,67,120,42,240,66,24,68,98,67,184,89,240,66,136,13,96,67,56,66,241,66,98,120,146,93,67,184,70,242,66,8,145,93,67,240,70,242,66,104,123,93,67,240,150,241,66,99,109,200,164,118,67,184,174,231,66,98,56,137,118,67,184,5,231,66,168,241,117,
        67,32,217,224,66,248,252,117,67,32,217,224,66,98,216,2,118,67,32,217,224,66,8,194,118,67,128,140,224,66,152,165,119,67,216,46,224,66,98,40,137,120,67,48,209,223,66,136,70,121,67,104,142,223,66,88,74,121,67,144,154,223,66,98,56,91,121,67,72,207,223,66,
        200,9,122,67,88,149,230,66,200,255,121,67,16,164,230,66,98,184,249,121,67,48,173,230,66,104,57,121,67,144,2,231,66,168,84,120,67,56,98,231,66,98,88,41,119,67,72,223,231,66,40,176,118,67,208,244,231,66,200,164,118,67,184,174,231,66,99,109,200,187,55,67,
        0,10,222,66,98,72,141,55,67,184,8,220,66,8,132,55,67,72,31,220,66,72,176,56,67,32,181,219,66,98,168,92,57,67,40,120,219,66,168,103,57,67,224,147,219,66,24,149,57,67,184,241,221,66,108,152,177,57,67,136,109,223,66,108,216,14,57,67,72,163,223,66,98,184,
        236,55,67,48,3,224,66,56,233,55,67,248,254,223,66,216,187,55,67,0,10,222,66,99,109,104,114,80,67,40,85,215,66,98,248,103,80,67,64,50,215,66,104,82,80,67,104,118,214,66,88,66,80,67,200,179,213,66,108,24,37,80,67,240,81,212,66,108,88,220,80,67,144,4,212,
        66,98,40,65,81,67,8,218,211,66,88,160,81,67,176,205,211,66,216,175,81,67,24,233,211,66,98,120,191,81,67,144,4,212,66,8,217,81,67,176,202,212,66,184,232,81,67,96,161,213,66,98,24,5,82,67,216,36,215,66,72,4,82,67,24,40,215,66,248,124,81,67,48,94,215,66,
        98,104,203,80,67,64,165,215,66,120,137,80,67,8,163,215,66,104,114,80,67,16,85,215,66,99,101,0,0};

    Path p;
    p.loadPathFromData (mainJuceLogo, sizeof (mainJuceLogo));
    return p;
};
