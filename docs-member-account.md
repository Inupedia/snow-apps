# Desktop member sign-in

InuShot uses the Yuntu member service independently of the existing Snow Shot
translation and update APIs. The product identity supplied for this integration
is `appCode=inu-capture`, `serviceId=11`. Login itself has no application or
service identifier; subscription queries send `serviceId=11`.

## Configuration and storage

- `SNOW_SHOT_MEMBER_API_BASE_URL` is a CMake cache setting, defaulting to
  `https://www.yuntudata.com.cn`. The environment variable of the same name can
  override it for development and isolated HTTP fixtures.
- Login: `POST /api/member/user/login`, with `telephone`, `password`, `mac`,
  and `locale`. HTTPS certificate validation stays enabled.
- Subscription: `POST /api/member/subscription/querySubscriptInfo`, with
  `auth-token`, `userId`, and `serviceId=11`.
- The current OS user owns the shared `YuntuSoftware/member` credential.
  macOS uses Keychain and Windows uses Credential Manager. There is no plaintext
  storage fallback. Passwords are stored only when Remember sign-in is selected,
  in `YuntuSoftware/member-secret`.
- Sign-out removes both shared account credentials and affects other Yuntu
  family applications when they next refresh their shared session.
- No test account, token, password, or device MAC is embedded in source code or
  application logs.

Sign-in is mandatory. The standalone login window appears before the application
controller is created. Main-window features, hotkeys, tray actions, capture, and
recording are unavailable until sign-in succeeds. Signing out tears down the
controller and returns to the login window. Closing that window exits the app.
Stored sessions older than the documented six-hour token lifetime require a new
login. Subscription status is independent of successful account authentication.

## Targeted verification

Build the `snow-shot-member-session-tests` target, then run:

```sh
ctest --test-dir build/snow-shot-macos-arm64-debug \
  -R '^snow-shot-member-session-tests$' --output-on-failure
```

The fixture uses an in-memory credential store and a local HTTP server. It checks
login field names, the `auth-token` header, service 11 subscription requests,
duplicate submission, shared session restore, unauthorized sessions, delayed
responses after sign-out, secure-storage failure, and unsupported credential
schemas. It never reads or writes the user's actual family credentials.

Desktop verification also requires the deployed signed app: keyboard submission,
blocked access before sign-in, credential errors, successful sign-in, restart with a restored session,
account status, and sign-out. Inspect the login window in light and dark
appearances and at a short window height. Use the repository run script after
changes so the signed deployed bundle is refreshed and restarted.

## Validation notes

The integration was exercised on macOS arm64 with the signed development app:
real sign-in, a restored session after restart, sign-out returning to the standalone
login window, and a screenshot hotkey that remains inactive while signed out.
The account UI fixtures cover light/dark rendering, obscured password input,
mandatory sign-in without a skip action, and scrolling in a short window.

At integration time, the documented subscription and Suite access POST routes
on the supplied host returned HTTP 405 from nginx. The application retains the
valid login and shows unavailable subscription status. Backend route publication
must be corrected before subscription status can be verified against that host.
