#include <JuceHeader.h>
#include <BinaryData.h>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>

using State = juce::AudioProcessorValueTreeState;
class VocalMetronomeEditor;
class VocalMetronome final : public juce::AudioProcessor
{
public:
    static State::ParameterLayout layout()
    {
        State::ParameterLayout l;
        l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync",1}, "Sync to DAW", true));
        l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"enabled",1}, "Enable counting", true));
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"bpm",1}, "Manual BPM", juce::NormalisableRange<float>(20,300,1),100));
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"volume",1}, "Volume", juce::NormalisableRange<float>(0,1,0.001f),0.85f));
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"pitch",1}, "Pitch", juce::NormalisableRange<float>(-12,12,1),0));
        for (int i=0;i<4;++i)
            l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mute"+juce::String(i),1}, "Mute beat "+juce::String(i+1), false));
        return l;
    }
    VocalMetronome() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)), state(*this,nullptr,"VocalMetronome",layout())
    {
        sync = state.getRawParameterValue("sync"); enabled=state.getRawParameterValue("enabled");
        bpm=state.getRawParameterValue("bpm"); volume=state.getRawParameterValue("volume"); pitch=state.getRawParameterValue("pitch");
        juce::WavAudioFormat format;
        const char* data[]={BinaryData::Count1_wav,BinaryData::Count2_wav,BinaryData::Count3_wav,BinaryData::Count4_wav};
        const int sizes[]={BinaryData::Count1_wavSize,BinaryData::Count2_wavSize,BinaryData::Count3_wavSize,BinaryData::Count4_wavSize};
        for(int i=0;i<4;++i)
        {
            mute[i]=state.getRawParameterValue("mute"+juce::String(i));
            std::unique_ptr<juce::AudioFormatReader> r(format.createReaderFor(new juce::MemoryInputStream(data[i],static_cast<size_t>(sizes[i]),false),true));
            if(!r) continue;
            auto& c=clips[i]; c.rate=r->sampleRate;
            c.audio.setSize(static_cast<int>(r->numChannels),static_cast<int>(r->lengthInSamples));
            r->read(&c.audio,0,c.audio.getNumSamples(),0,true,true);
            int first=0;
            for(;first<c.audio.getNumSamples();++first)
            {
                bool loud=false;
                for(int ch=0;ch<c.audio.getNumChannels();++ch) loud |= std::abs(c.audio.getSample(ch,first))>0.015f;
                if(loud) break;
            }
            c.offset=juce::jmax(0,first-static_cast<int>(0.008*c.rate));
        }
    }
    const juce::String getName() const override { return "VocalMetronome"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    bool isBusesLayoutSupported(const BusesLayout& b) const override
    { return b.getMainInputChannelSet().isDisabled() && (b.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()); }
    void prepareToPlay(double rate,int) override { sampleRate=rate; resetClock(); }
    void releaseResources() override { resetClock(); }
    void reset() override { resetClock(); }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock& dest) override
    { if(auto xml=state.copyState().createXml()) copyXmlToBinary(*xml,dest); }
    void setStateInformation(const void* data,int size) override
    { if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml)); }
    void processBlock(juce::AudioBuffer<float>& out,juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        out.clear(); midi.clear();
        const bool hostSync=sync->load()>0.5f;
        double tempo=bpm->load(), q=manualQ;
        bool running=enabled->load()>0.5f, valid=true;
        if(hostSync)
        {
            valid=false;
            if(auto* ph=getPlayHead()) if(auto pos=ph->getPosition())
            {
                const auto t=pos->getBpm(); const auto p=pos->getPpqPosition();
                if(t && p && std::isfinite(*t) && *t>0 && std::isfinite(*p))
                { tempo=*t; q=*p; valid=true; }
                running=running && pos->getIsPlaying();
            }
            running=running && valid;
        }
        displayedBpm.store(tempo); hostAvailable.store(valid);
        playing.store(running);
        if(!running) { resetClock(); previousSync=hostSync; return; }
        const double step=tempo/(60.0*sampleRate);
        const bool discontinuity=!wasRunning || previousSync!=hostSync || (hostSync && std::abs(q-expectedQ)>juce::jmax(2.0*step,1.0e-5));
        if(discontinuity)
        {
            voice=-1;
            const auto tick=static_cast<int64_t>(std::floor(q+1.0e-8));
            // Starting between beats waits for the next grid line; exact beats play immediately.
            lastTick=std::abs(q-static_cast<double>(tick))<1.0e-7 ? tick-1 : tick;
        }
        wasRunning=true; previousSync=hostSync;
        const double pitchRate=std::pow(2.0,static_cast<double>(pitch->load())/12.0);
        const float level=volume->load();
        for(int s=0;s<out.getNumSamples();++s)
        {
            const double now=q+static_cast<double>(s)*step;
            const auto tick=static_cast<int64_t>(std::floor(now+1.0e-8));
            if(tick!=lastTick)
            {
                lastTick=tick; voice=static_cast<int>((tick%4+4)%4);
                position=clips[voice].offset; age=0;
                duration=juce::jmax(1,static_cast<int>(std::ceil((static_cast<double>(tick)+1.0-now)/step)));
                activeBeat.store(voice);
            }
            if(voice<0) continue;
            const auto& c=clips[voice];
            const int a=static_cast<int>(position);
            if(a>=c.audio.getNumSamples() || age>=duration) { voice=-1; continue; }
            const int b=juce::jmin(a+1,c.audio.getNumSamples()-1);
            const float f=static_cast<float>(position-a);
            const float gain=level*(mute[voice]->load()>0.5f ? 0.0f : 1.0f)
                *juce::jlimit(0.0f,1.0f,static_cast<float>(duration-age)/static_cast<float>(juce::jmax(1.0,0.012*sampleRate)));
            for(int ch=0;ch<out.getNumChannels();++ch)
            {
                const int src=juce::jmin(ch,c.audio.getNumChannels()-1);
                if(src>=0) out.setSample(ch,s,(c.audio.getSample(src,a)*(1-f)+c.audio.getSample(src,b)*f)*gain);
            }
            position+=c.rate/sampleRate*pitchRate; ++age;
        }
        expectedQ=q+static_cast<double>(out.getNumSamples())*step;
        manualQ=expectedQ;
    }
    State state;
    std::atomic<double> displayedBpm{100};
    std::atomic<int> activeBeat{-1};
    std::atomic<bool> playing{false},hostAvailable{false};
private:
    struct Clip { juce::AudioBuffer<float> audio; double rate=44100; int offset=0; };
    std::array<Clip,4> clips;
    std::atomic<float> *sync{},*enabled{},*bpm{},*volume{},*pitch{};
    std::array<std::atomic<float>*,4> mute{};
    double sampleRate=44100,manualQ=0,expectedQ=0,position=0;
    int64_t lastTick=std::numeric_limits<int64_t>::min();
    int voice=-1,age=0,duration=0;
    bool wasRunning=false,previousSync=true;
    void resetClock() { voice=-1; wasRunning=false; manualQ=0; expectedQ=0; activeBeat.store(-1); playing.store(false); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalMetronome)
};

class VocalLook final : public juce::LookAndFeel_V4
{
public:
    VocalLook()
    {
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff28343e));
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff43303c));
        setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        setColour(juce::Slider::thumbColourId,juce::Colour(0xff81bdb7));
        setColour(juce::Slider::trackColourId,juce::Colour(0xff81bdb7));
        setColour(juce::Slider::backgroundColourId,juce::Colour(0xff34414d));
        setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffdce4eb));
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId,juce::Colour(0xffdce4eb));
        setColour(juce::ToggleButton::textColourId,juce::Colour(0xffdce4eb));
        setColour(juce::ToggleButton::tickColourId,juce::Colour(0xff81bdb7));
        setColour(juce::ToggleButton::tickDisabledColourId,juce::Colour(0xffa3b2c0));
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff222d37));
    }
};
class VocalMetronomeEditor final : public juce::AudioProcessorEditor,private juce::Timer
{
public:
    explicit VocalMetronomeEditor(VocalMetronome& processor) : AudioProcessorEditor(processor),p(processor)
    {
        setLookAndFeel(&look);
        addAndMakeVisible(title); title.setText("METRONOME",juce::dontSendNotification);
        title.setFont(juce::Font(32.0f,juce::Font::bold)); title.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(tempo); tempo.setFont(juce::Font(54.0f,juce::Font::bold)); tempo.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(unit); unit.setText("BEATS PER MINUTE",juce::dontSendNotification); unit.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(syncButton); syncButton.setButtonText("Sync to FL Studio / DAW");
        syncAttachment=std::make_unique<State::ButtonAttachment>(p.state,"sync",syncButton);
        setupSlider(bpmSlider,"bpm",bpmAttachment,false);
        setupSlider(volumeSlider,"volume",volumeAttachment,true);
        setupSlider(pitchSlider,"pitch",pitchAttachment,true);
        addAndMakeVisible(volumeLabel); volumeLabel.setText("Volume",juce::dontSendNotification);
        addAndMakeVisible(pitchLabel); pitchLabel.setText("Pitch",juce::dontSendNotification);
        for(int i=0;i<4;++i)
        {
            addAndMakeVisible(beatLabels[i]); beatLabels[i].setText(juce::String(i+1),juce::dontSendNotification);
            beatLabels[i].setFont(juce::Font(26.0f,juce::Font::bold)); beatLabels[i].setJustificationType(juce::Justification::centred);
            addAndMakeVisible(mutes[i]); mutes[i].setButtonText("Mute"); mutes[i].setClickingTogglesState(true);
            muteAttachments[i]=std::make_unique<State::ButtonAttachment>(p.state,"mute"+juce::String(i),mutes[i]);
        }
        addAndMakeVisible(enableButton); enableButton.setClickingTogglesState(true);
        enableButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff81bdb7));
        enableButton.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff122c2c));
        enableAttachment=std::make_unique<State::ButtonAttachment>(p.state,"enabled",enableButton);
        addAndMakeVisible(resetPitch); resetPitch.setButtonText("Reset pitch");
        resetPitch.onClick=[this]{auto* a=p.state.getParameter("pitch"); a->beginChangeGesture(); a->setValueNotifyingHost(a->convertTo0to1(0)); a->endChangeGesture();};
        addAndMakeVisible(status); status.setJustificationType(juce::Justification::centred);
        setSize(480,590); startTimerHz(30);
    }
    ~VocalMetronomeEditor() override { stopTimer(); setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a2028));
        
        for(int i=0;i<4;++i)
        {
            const bool on=p.playing.load() && p.activeBeat.load()==i;
            g.setColour(juce::Colour(on?0xff81bdb7:0xff222d37));
            g.fillRoundedRectangle(beatLabels[i].getBounds().toFloat(),12.0f);
        }
    }
    void resized() override
    {
        title.setBounds(24,22,432,44); syncButton.setBounds(118,82,260,30);
        tempo.setBounds(90,118,300,70); unit.setBounds(90,188,300,24); bpmSlider.setBounds(32,224,416,34);
        for(int i=0;i<4;++i) { beatLabels[i].setBounds(32+i*107,280,95,60); mutes[i].setBounds(32+i*107,350,95,30); }
        enableButton.setBounds(32,400,416,44);
        volumeLabel.setBounds(32,462,65,28); volumeSlider.setBounds(102,462,346,28);
        pitchLabel.setBounds(32,504,65,28); pitchSlider.setBounds(102,504,250,28); resetPitch.setBounds(356,504,92,28);
        status.setBounds(24,548,432,26);
    }
private:
    VocalMetronome& p; VocalLook look;
    juce::Label title,tempo,unit,volumeLabel,pitchLabel,status;
    juce::ToggleButton syncButton;
    juce::TextButton enableButton,resetPitch;
    juce::Slider bpmSlider,volumeSlider,pitchSlider;
    std::array<juce::Label,4> beatLabels;
    std::array<juce::TextButton,4> mutes;
    std::unique_ptr<State::ButtonAttachment> syncAttachment,enableAttachment;
    std::array<std::unique_ptr<State::ButtonAttachment>,4> muteAttachments;
    std::unique_ptr<State::SliderAttachment> bpmAttachment,volumeAttachment,pitchAttachment;
    void setupSlider(juce::Slider& s,const juce::String& id,std::unique_ptr<State::SliderAttachment>& attachment,bool text)
    {
        addAndMakeVisible(s); s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(text?juce::Slider::TextBoxRight:juce::Slider::NoTextBox,false,55,24);
        attachment=std::make_unique<State::SliderAttachment>(p.state,id,s);
    }
    void timerCallback() override
    {
        const bool sync=p.state.getRawParameterValue("sync")->load()>0.5f;
        const bool enabled=p.state.getRawParameterValue("enabled")->load()>0.5f;
        bpmSlider.setEnabled(!sync);
        tempo.setText(juce::String(sync?p.displayedBpm.load():bpmSlider.getValue(),1),juce::dontSendNotification);
        enableButton.setButtonText(enabled?"Counting enabled — click to mute":"Counting disabled — click to enable");
        status.setText(sync?(!p.hostAvailable.load()?"Waiting for DAW timing":(p.playing.load()?"Following DAW · 1–2–3–4":"Waiting for DAW playback")):(enabled?"Manual tempo · 1–2–3–4":"Stopped"),juce::dontSendNotification);
        for(int i=0;i<4;++i)
            beatLabels[i].setColour(juce::Label::textColourId,p.playing.load()&&p.activeBeat.load()==i?juce::Colour(0xff122c2c):juce::Colour(0xffbfccd6));
        repaint();
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalMetronomeEditor)
};
juce::AudioProcessorEditor* VocalMetronome::createEditor() { return new VocalMetronomeEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalMetronome(); }
