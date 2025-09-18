/*
  ==============================================================================

    AuthPropertiesMenu.cpp
    Created: 17 Sep 2025 9:36:57pm
    Author:  ikamo

  ==============================================================================
*/

#include "AuthPropertiesMenu.h"
#include "PluginProcessor.h"
#include "RestAPIHandler.h"
#include <random>
#include <openssl/sha.h>

// ================== PKCE Helpers ==================

static juce::String generateCodeVerifier()
{
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789-._~";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    juce::String result;
    for (int i = 0; i < 64; ++i)
        result << charset[dist(gen)];
    return result;
}

static juce::String base64UrlEncode(const unsigned char* data, size_t len)
{
    juce::MemoryBlock mb(data, len);
    auto b64 = mb.toBase64Encoding();

    b64 = b64.replaceCharacter('+', '-')
        .replaceCharacter('/', '_');

    while (b64.endsWithChar('='))
        b64 = b64.dropLastCharacters(1);

    return b64;
}

static juce::String generateCodeChallenge(const juce::String& verifier)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(verifier.toRawUTF8()),
        verifier.getNumBytesAsUTF8(), hash);
    return base64UrlEncode(hash, SHA256_DIGEST_LENGTH);
}

// ================== OAuth Browser ==================

class OAuthBrowser : public juce::WebBrowserComponent
{
public:
    OAuthBrowser(const juce::String& startUrl,
        std::function<void(const juce::String&)> onResult)
        : onResultCallback(std::move(onResult))
    {
        goToURL(startUrl);
    }

    bool pageAboutToLoad(const juce::String& newUrl) override
    {
        if (newUrl.startsWith("myapp://oauth/callback") || newUrl.contains("code="))
        {
            if (onResultCallback)
                onResultCallback(newUrl);

            if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
                w->closeButtonPressed();

            return false;
        }
        return true;
    }

private:
    std::function<void(const juce::String&)> onResultCallback;
};

// ================== AuthPropertiesMenu ==================

struct PendingOAuth
{
    juce::String codeVerifier;
    juce::String state;
};

static std::optional<PendingOAuth> pendingOAuth;

AuthPropertiesMenu::AuthPropertiesMenu(WaviateFlow2025AudioProcessor& p)
    : processor(p)
{
    providerButtons.reserve(8);
    setButtonIcon(BinaryData::AuthLogo_png, BinaryData::AuthLogo_pngSize);

    auto makeButton = [this](const juce::String& name, const juce::Colour& colour, auto onClick)
        {
            auto btn = std::make_unique<juce::TextButton>(name);
            btn->setColour(juce::TextButton::buttonColourId, colour.darker());
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn->onClick = onClick;

            providerButtons.push_back(std::move(btn));
            addAndMakeVisible(providerButtons.back().get());
        };

    makeButton("Google", juce::Colours::mediumaquamarine, [this] { launchOAuth("google"); });
    makeButton("Discord", juce::Colours::mediumpurple, [this] { launchOAuth("discord"); });
    makeButton("GitHub", juce::Colours::grey, [this] { launchOAuth("github"); });
    makeButton("Facebook", juce::Colours::cornflowerblue, [this] { launchOAuth("facebook"); });
}

void AuthPropertiesMenu::onUpdateUI()
{
    // Later: update to show "Logged in as <user>"
}

void AuthPropertiesMenu::paint(juce::Graphics& g) {
    g.setColour(juce::Colours::black);
    g.fillAll();
}

void AuthPropertiesMenu::resized()
{
    auto area = getLocalBounds().reduced(10);
    int h = 30;
    for (auto& btn : providerButtons)
    {
        btn->setBounds(area.removeFromTop(h).reduced(0, 4));
        area.removeFromTop(5);
    }
}

void AuthPropertiesMenu::launchOAuth(const juce::String& provider)
{
    juce::String clientId;
    juce::String redirectUri = "myapp://oauth/callback";
    juce::String url;

    auto verifier = generateCodeVerifier();
    auto challenge = generateCodeChallenge(verifier);
    juce::String state = juce::Uuid().toString();

    pendingOAuth = PendingOAuth{ verifier, state };

    if (provider == "google")
    {
        clientId = "YOUR_GOOGLE_CLIENT_ID";
        url = "https://accounts.google.com/o/oauth2/v2/auth?"
            "client_id=" + clientId +
            "&redirect_uri=" + redirectUri +
            "&response_type=code"
            "&scope=openid%20email%20profile"
            "&access_type=offline"
            "&code_challenge=" + challenge +
            "&code_challenge_method=S256"
            "&state=" + state;
    }
    else if (provider == "discord")
    {
        clientId = "YOUR_DISCORD_CLIENT_ID";
        url = "https://discord.com/api/oauth2/authorize?"
            "client_id=" + clientId +
            "&redirect_uri=" + redirectUri +
            "&response_type=code"
            "&scope=identify%20email"
            "&code_challenge=" + challenge +
            "&code_challenge_method=S256"
            "&state=" + state;
    }
    else if (provider == "github")
    {
        clientId = "YOUR_GITHUB_CLIENT_ID";
        url = "https://github.com/login/oauth/authorize?"
            "client_id=" + clientId +
            "&redirect_uri=" + redirectUri +
            "&scope=read:user%20user:email"
            "&allow_signup=true"
            "&code_challenge=" + challenge +
            "&code_challenge_method=S256"
            "&state=" + state;
    }
    else if (provider == "facebook")
    {
        clientId = "YOUR_FACEBOOK_CLIENT_ID";
        url = "https://www.facebook.com/v12.0/dialog/oauth?"
            "client_id=" + clientId +
            "&redirect_uri=" + redirectUri +
            "&response_type=code"
            "&scope=email,public_profile"
            "&code_challenge=" + challenge +
            "&code_challenge_method=S256"
            "&state=" + state;
    }

    auto* browser = new OAuthBrowser(url, [this, provider](juce::String callbackUrl)
        {
            juce::URL parsed(callbackUrl);
            auto allParams = parsed.getParameterValues();
            auto allNames = parsed.getParameterNames();
            std::unordered_map<juce::String, juce::String> parms;
            for (int i = 0; i < allParams.size(); i += 1) {
                parms.insert_or_assign(allNames[i], allParams[i]);
            }
            auto code = parms.at("code");
            auto state = parms.at("state");

            if (!pendingOAuth || state != pendingOAuth->state)
            {
                DBG("Invalid OAuth state!");
                return;
            }

            exchangeCodeForToken(provider, code, pendingOAuth->codeVerifier);
        });

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(browser);
    opts.dialogTitle = "Login with " + provider;
    opts.dialogBackgroundColour = juce::Colours::black;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.launchAsync();
}

static std::string urlEncode(const std::string& value)
{
    juce::String juceStr(value);
    return juce::URL::addEscapeChars(juceStr, true).toStdString();
}

static std::string formEncode(const std::map<std::string, std::string>& params)
{
    std::ostringstream oss;
    bool first = true;
    for (auto& [k, v] : params)
    {
        if (!first) oss << "&";
        first = false;
        oss << urlEncode(k) << "=" << urlEncode(v);
    }
    return oss.str();
}


void AuthPropertiesMenu::exchangeCodeForToken(const juce::String& provider,
    const juce::String& code,
    const juce::String& verifier)
{
    std::string tokenUrl;
    std::map<std::string, std::string> headers = {
        { "Content-Type", "application/x-www-form-urlencoded" }
    };
    std::map<std::string, std::string> body;

    std::string clientId;
    std::string redirectUri = "myapp://oauth/callback";

    if (provider == "google")
    {
        clientId = "YOUR_GOOGLE_CLIENT_ID";
        tokenUrl = "https://oauth2.googleapis.com/token";
        body = {
            { "code", code.toStdString() },
            { "client_id", clientId },
            { "redirect_uri", redirectUri },
            { "grant_type", "authorization_code" },
            { "code_verifier", verifier.toStdString() }
        };
    }
    else if (provider == "discord")
    {
        clientId = "YOUR_DISCORD_CLIENT_ID";
        tokenUrl = "https://discord.com/api/oauth2/token";
        body = {
            { "code", code.toStdString() },
            { "client_id", clientId },
            { "redirect_uri", redirectUri },
            { "grant_type", "authorization_code" },
            { "code_verifier", verifier.toStdString() }
        };
    }
    else if (provider == "github")
    {
        clientId = "YOUR_GITHUB_CLIENT_ID";
        tokenUrl = "https://github.com/login/oauth/access_token";
        body = {
            { "code", code.toStdString() },
            { "client_id", clientId },
            { "redirect_uri", redirectUri },
            { "grant_type", "authorization_code" },
            { "code_verifier", verifier.toStdString() }
        };
        headers["Accept"] = "application/json"; // GitHub quirk
    }
    else if (provider == "facebook")
    {
        clientId = "YOUR_FACEBOOK_CLIENT_ID";
        tokenUrl = "https://graph.facebook.com/v12.0/oauth/access_token";
        body = {
            { "code", code.toStdString() },
            { "client_id", clientId },
            { "redirect_uri", redirectUri },
            { "grant_type", "authorization_code" },
            { "code_verifier", verifier.toStdString() }
        };
    }

    std::string form = formEncode(body);

    std::string response = RestApiHandler::post(tokenUrl, form, headers, 3);
    if (response.empty())
    {
        DBG("OAuth token exchange failed for " << provider);
        return;
    }

    juce::var result = juce::JSON::parse(response);
    if (result.isObject())
    {
        auto* obj = result.getDynamicObject();
        auto token = obj->getProperty("access_token").toString();
        DBG("Access token for " << provider << ": " << token);

        // TODO: Save token (e.g. processor.setAccessToken(provider, token))
    }
    else
    {
        DBG("OAuth response not JSON: " << response);
    }
}

