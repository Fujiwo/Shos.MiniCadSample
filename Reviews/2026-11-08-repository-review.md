# Shos.MiniCadSample リポジトリ全体レビュー

- レビュー日: 2026-11-08
- 対象コミット: `ae5c882`（main）
- 対象範囲: `Shos.MiniCadSample`（アプリ本体）、`Shos.MiniCadSample.Test`、`Shos.MiniCadSample.MemoryLeakTest`、プロジェクト設定、README
- 追記: 作業ツリー上で更新された `undo_redo_vector.h` を、8 章で再レビューしました。M7 と M8 は解消済みです。
- 追記: 作業ツリー上で更新された `DoubleBuffer.h` / `DoubleBuffer.cpp` / `GdiObjectSelector.h` / `View.h` を、9 章で再レビューしました。
- 方法: ソースを通読して静的に検討しました。**ビルドも実行もしていません**（WSL 上で作業したため）。「再現手順」は MFC と Win32 の既知の動作から推定したものです。「要実機確認」と書いた項目は、実際に動かして確かめてください。
- 追記（2026-11-08）: リポジトリ所有者が実機で確認し、**H2、M2、M5 の 3 件とも再現しました**。

---

## 1. 総評

サンプルとしては設計がよく整理されています。

- Model と View が分かれており、Observer パターンで `Hint` を通知して差分を再描画しています。
- マウス入力は `MouseEventTranslator` で「クリック／カーソル移動／ドラッグ」という意味のあるイベントに変換されます。
- 図形の追加は Command クラスで切り替えられ、Undo/Redo は汎用コンテナ `undo_redo_vector` に任せています。

責務の切り方は教材として読みやすく、拡張もしやすい構造です。

一方、レイヤーの**境界で状態の一貫性が崩れる**問題がいくつかあります。具体的には、Undo 履歴と図形の寿命、ダイアログと Model の属性の受け渡し、`OnInitialUpdate` の再実行です。特に次の 4 件は、普通の操作で起きるうえ、データ消失や二重処理につながるため優先して直すことを勧めます。

1. **[H1]** 切り取り（Cut）で、選択とは無関係に**全図形が消え**、Undo もできず、変更フラグも立たない
2. **[H2]** 新規作成や「開く」をするたびにリスナーとオブザーバーが**重複登録**され、1 回のクリックで図形が複数追加される
3. **[H3]** 色の異なる図形を複数選んで色を変えると、**線幅まで 1 つ目の図形の値に上書き**される
4. **[H4]** ハイライト中の図形へのポインタが Undo や削除の後も残り、**解放済みメモリを描画しうる**

---

## 2. 指摘一覧

| ID | 重大度 | 分類 | 場所 | 概要 |
|---|---|---|---|---|
| H1 | 高 | 正しさ / データ消失 | `ClipboardHelper.h:23-31` | Cut が全図形を削除し、Undo 不可、変更フラグなし |
| H2 | 高 | 正しさ | `View.h:47-62` | `OnInitialUpdate` のたびにリスナーとオブザーバーを重複登録する（**実機で再現を確認**） |
| H3 | 高 | 正しさ / データ破壊 | `FigureAttributeDialog.h:42-46`, `Model.h:64-85` | 複数選択で属性を変えると、無効な値（線幅など）まで適用される |
| H4 | 高 | メモリ安全性 | `Model.h:203`, `Command.h:222` | `highlightedFigure` が無効化されず、解放済みポインタを参照しうる |
| M1 | 中 | 正しさ | `Model.h:213-229` | ファイルを読み込むと、読み込んだ図形が Undo 履歴に積まれる |
| M2 | 中 | 正しさ | `ClipboardHelper.h:33-43` | 貼り付けた図形が表示されない（描画キャッシュが無効化されない）、Undo が 1 図形ずつになる（**表示されない点は実機で再現を確認**） |
| M3 | 中 | 正しさ | `Zooming.h:37-50` | ホイールズームの中心がずれる（スクリーン座標のまま使い、変換結果も捨てている） |
| M4 | 中 | 正しさ | `ClipboardHelper.h:77,82` | 独自形式に `CF_PRIVATEFIRST` を使っているため、他アプリのデータを読み込んで例外になりうる |
| M5 | 中 | UI | `FigureAttributeDialog.cpp:54-63` | `OnCommand` が基底クラスを呼ばないため、× ボタンや Esc でダイアログを閉じられない（**実機で再現を確認**） |
| M6 | 中 | 正しさ（潜在） | `Model.h:112-122` | `Model::Remove` が `erase` で無効になったイテレータを参照する（現在は未使用） |
| M7 | 中 | 堅牢性 | `undo_redo_vector.h:254-258, 398-402` | ~~`clean_up` なしで生成した `undo_redo_vector` の `reset()` が null 参照でクラッシュする~~ **解消済み（8 章）** |
| M8 | 中 | 堅牢性 | `undo_redo_vector.h:308-343` | ~~`transaction` のデストラクタから例外を投げうる。コピーも禁止されていない~~ **解消済み（8 章）** |
| L1 | 低 | 正しさ | `MouseEventTranslator.h:106-113` | 右クリック（ドラッグなし）が左クリックと同じ扱いになる |
| L2 | 低 | 正しさ | `Document.h:102-106` | 描画領域の外でドラッグを終えると、範囲選択の枠や入力途中の点が残る |
| L3 | 低 | 正しさ | `Model.h:124-137` | 何も選択していなくても Delete で変更フラグが立つ |
| L4 | 低 | 正しさ | `Model.h:84` | 属性変更の `Hint` が古い図形を渡すため、線幅を太くしたときの再描画範囲が足りない |
| L5 | 低 | 正しさ（潜在） | `Observer.h:23-26` | `RemoveObserver` が `std::remove` だけで `erase` しておらず、実際には削除されない |
| L6 | 低 | 寿命 | `Model.h:55-58` | `~Model` → `Reset` → 通知という流れで、破棄途中の `Document` やメインウィンドウを触る |
| L7 | 低 | デバッグコード | `MouseEventTranslator.h:46` | `TRACE` の書式と引数がずれている |
| T1 | 低 | テスト | `Shos.MiniCadSample.Test.cpp:22` | `check_vector` のループが 1 回も回らない |
| T2 | 低 | テスト | `Shos.MiniCadSample.MemoryLeakTest.cpp:87-93` | `undo_redo_pointer_vector_memory_leak_test` が呼ばれていない |
| T3 | 低 | テスト | テスト全般 | 引数の順序（期待値と実際値）が逆。アサーションのないテストがある。`reset()` などが未テスト |

この後に詳細と修正案を書きます。

---

## 3. 詳細

### H1. Cut が全図形を削除し、Undo 不可、変更フラグなし

**場所**: `ClipboardHelper.h:23-31`（`OnEditCut`）

```cpp
if (OnEditCopy(document, view, size, backgroundColor, draw)) {
    document.DeleteContents();   // ← Model::Reset() が呼ばれる
```

- `OnEditCopy` は**ドキュメント全体**をシリアライズしてクリップボードに入れます。選択中の図形だけではありません。
- 続く `DeleteContents()` は `Model::Reset()` を呼び、図形だけでなく **Undo 履歴も**破棄します。
- `Reset` が発行する通知は `Hint::Type::ViewOnly` だけです。そのため `SetModifiedFlag()` が呼ばれません。

**結果**: Ctrl+X（または Shift+Del）を押すと全図形が消え、Ctrl+Z で戻せません。変更フラグも立たないので、そのまま閉じると保存確認が出ず、ファイルとの差分に気づけません。

**修正案**:
- Cut は「選択中の図形だけをコピーし、`Model::RemoveSelectedFigures()` で削除する」形にする。Undo できる操作として扱われます。
- Copy も、選択中の図形があればそれだけを対象にする（クリップボード用のシリアライズを `Model` 側に用意する）。
- `ON_UPDATE_COMMAND_UI` を追加し、何も選択されていないときは Cut と Copy を無効にする。

---

### H2. `OnInitialUpdate` のたびにリスナーとオブザーバーを重複登録する

**場所**: `View.h:47-62`

```cpp
Application::SetFigureAttributeObserver(GetDocument().GetModel()); // ダイアログ → Model
...
mouseEventTranslator.AddListener(GetDocument());
mouseEventTranslator.AddListener(*this);
```

SDI（`CSingleDocTemplate`）では、「ファイル → 新規作成」や「開く」のたびに、同じ View に対して `InitialUpdateFrame` → `OnInitialUpdate` が呼ばれ直します。登録処理には重複チェックがありません。

**結果**（N 回目の新規作成または開く操作の後）:
- 1 回のクリックが `Document` に N 回届く。たとえば Dot コマンドで 1 回クリックすると、点が N 個追加される。
- ダイアログで属性を 1 回変えると `Model::Update` が N 回走り、Undo のトランザクションも N 個積まれる。

**修正案**:
- 登録は `OnCreate` などの 1 回だけ実行される場所に移す。または登録済みフラグで防ぐ。
- `AddListener` / `AddObserver` 側で重複を無視する（`std::find` で確認してから `push_back`）。
- あわせて L5 を直し、解除できるようにする。

---

### H3. 複数選択で属性を変えると、無効な値まで適用される

**場所**: `FigureAttributeDialog.h:42-46`, `FigureAttributeDialog.cpp:31-42`, `Model.h:64-85`

- 複数選択したときは `FigureAttribute::GetSum` が値の混在を `isColorValid` / `isPenWidthValid = false` で表します。ただし `color` と `penWidth` の値そのものは 1 つ目の図形のものが残ります。
- ダイアログで色だけを変えても、`NotifyObservers(figureAttribute)` は**属性全体**を送ります。
- `Model::Update(selectedFigures, figureAttribute)` は `updatedFigure->Attribute() = figureAttribute;` で属性全体を代入します。

**結果**:
- 線幅の異なる図形を複数選んで色だけを変えると、**全員の線幅が 1 つ目の図形の線幅になります**。色を変えずに線幅だけを変えた場合も同様です。
- `isXxxValid = false` というフラグが図形自身の属性にコピーされます。その後その図形を 1 つだけ選択しても、ダイアログの色が「自動」になり線幅が空欄になります。
- `SetColor` / `SetPenWidth` は有効フラグを `true` に戻しません。

**修正案**:
- 「どの項目が変更されたか」を通知に含め、Model 側では変更された項目だけを各図形に適用する。たとえば `FigureAttribute` に `ApplyTo(FigureAttribute& target)` を用意し、有効な項目だけを上書きする。
- `SetColor` / `SetPenWidth` で対応する有効フラグを `true` にする。
- 図形が持つ属性には有効フラグを持たせない（表示用の集計結果と、図形自身の属性を別の型にする）。

---

### H4. `highlightedFigure` が無効化されず、解放済みポインタを参照しうる

**場所**: `Model.h:203-211`, `Command.h:222-223`

`Model::highlightedFigure` を更新するのは `SelectCommand::OnCursorMove` と `Model::Reset` だけです。Undo、Redo、削除、属性変更でモデルから外れた図形を指したまま残ります。

`undo_redo_pointer_vector` は、モデルから外れた図形を Undo ステップが持っている間は生かしておきます。しかし Redo 履歴が切り捨てられた時点（新しい操作が積まれたとき）で `delete` します。

**再現手順（推定）**:
1. 選択モードで、直前に追加した図形 F にカーソルを合わせる（F がハイライトされる）。
2. Ctrl+Z を押す。F がモデルから外れるが、ハイライトは F のまま残り、図形のない場所に枠だけが描かれる。
3. マウスをビューの上で動かさずに、キーボードでメニューから「Figure → Random」などを選ぶ。Redo 履歴が切り捨てられ、F が `delete` される。
4. 再描画で `SelectCommand::OnDraw` → `Hilight()->DrawArea(dc)` が呼ばれ、**解放済みメモリにアクセス**する。

**修正案**:
- `Model` で Undo、Redo、削除、変更、追加のたびに `highlightedFigure` を検証する。モデル内に存在しなければ `nullptr` にする。単純に、図形を変更する操作のたびに `nullptr` にするだけでも足ります。
- 長期的には、ポインタではなく ID で図形を参照する設計にする。

---

### M1. ファイルを読み込むと、読み込んだ図形が Undo 履歴に積まれる

**場所**: `Model.h:213-229`

読み込み時は `figures.push_back(figure)` を呼んでいるため、図形ごとに `add` ステップが積まれます。

**結果**: ファイルを開いた直後から Undo が有効になっています。Ctrl+Z を押すと、読み込んだ図形が 1 つずつ消え、変更フラグも立ちます。

**修正案**: `undo_redo_collection` に「履歴を残さずに要素を追加する」か「現在の内容を保ったまま履歴だけを消す」API を用意し、読み込みの後に呼ぶ。現在の `reset()` は要素まで解放するので、この用途には使えません。

---

### M2. 貼り付けた図形が表示されない、Undo が 1 図形ずつになる

**場所**: `ClipboardHelper.h:33-43, 80-95`

- 貼り付けでは `document.Serialize(ar)` → `Model::Serialize` と進み、図形が追加されます。しかし **`NotifyObservers` が呼ばれません**。
- `view.Invalidate()` は呼ばれますが、`DoubleBuffer` の図形キャッシュ（Layer1）を無効にする `Update()` は呼ばれません。そのため、ウィンドウのサイズ変更などでキャッシュが作り直されるまで、貼り付けた図形が表示されない可能性が高いです（**実機で再現を確認**）。
- 図形ごとに Undo ステップが積まれるため、1 回の貼り付けを取り消すのに図形の数だけ Ctrl+Z が必要です。
- `SetModifiedFlag()` を 2 回呼んでいます（`AddDataFromClipboard` と `OnEditPaste`）。

**修正案**: `Model` に `Paste(CArchive&)` のような API を用意し、トランザクションの中で追加したうえで `Hint::Type::Added` を通知する。

---

### M3. ホイールズームの中心がずれる

**場所**: `Zooming.h:37-50`, `View.h`（`OnMouseWheel`）

```cpp
DPtoLP(point);   // 戻り値を捨てている
...
Geometry::Enlarge(newLogicalArea, point, ...);  // point はデバイス座標のまま
```

- `Zooming::DPtoLP` は値を返す関数ですが、戻り値を使っていません。
- `WM_MOUSEWHEEL` の座標は**スクリーン座標**です。クライアント座標への変換（`ScreenToClient`）も行っていません。

**結果**: Ctrl+ホイールで拡大縮小すると、マウスの位置ではなく、ずれた位置を中心にズームします。

**修正案**:

```cpp
window.ScreenToClient(&point);
point = DPtoLP(point);
```

---

### M4. 独自形式に `CF_PRIVATEFIRST` を使っている

**場所**: `ClipboardHelper.h:77, 82`

`CF_PRIVATEFIRST` 〜 `CF_PRIVATELAST` は、どのアプリも自由に使える範囲です。この値だけでは自アプリのデータだと判別できません。他のアプリが同じ番号でデータを置いていると、それを `CArchive` で読み込み、`CArchiveException` などの例外になります。`AddDataFromClipboard` には `try/catch` がありません。

**修正案**: `::RegisterClipboardFormat(_T("Shos.MiniCadSample.Figures"))` で一意の形式を登録して使う。その場合、データの解放はシステムが行うため、`WM_DESTROYCLIPBOARD` での手動解放（`globalMemoryHandle`）も不要になります。読み込みは `try/catch` で囲みます。

---

### M5. ダイアログを × ボタンや Esc で閉じられない（実機で再現を確認）

**場所**: `FigureAttributeDialog.cpp:54-63`

`OnCommand` のオーバーライドが基底の `CDialogEx::OnCommand` を呼ばず、常に `0` を返しています。× ボタン（`WM_CLOSE`）や Esc は、`DefDlgProc` によって `WM_COMMAND(IDCANCEL)` に変換されます。しかしそのコマンドがここで止まるため、オーバーライドした `OnCancel()`（ダイアログを隠す処理）が呼ばれません。Enter（`IDOK`）でモードレスダイアログを `EndDialog` させないための処置だと思われますが、副作用が大きいです。

**修正案**: `IDC_COLOR_BUTTON` / `IDC_PEN_WIDTH_BUTTON` の処理の後に、`return CDialogEx::OnCommand(wParam, lParam);` を呼ぶ。`OnOK` は別にオーバーライドして、何もしないか値を適用するだけにする。

---

### M6. `Model::Remove` が無効になったイテレータを参照する（潜在バグ）

**場所**: `Model.h:112-122`

```cpp
figures.erase(iterator);
if ((*iterator)->IsSelected())   // erase 後のイテレータを参照している
```

`erase` の後の `iterator` は無効です。実際には次の要素を指すか、末尾なら `end()` を指して範囲外を参照します。図形が見つからなかった場合の `erase(end())` も未定義動作です。

現在は呼び出し元がない（`Model::Change` と `Document::Add` も同様）ため、表に出ていません。使うなら修正し、使わないなら削除することを勧めます。

---

### M7. `clean_up` なしの `undo_redo_vector` で `reset()` するとクラッシュする

**場所**: `undo_redo_vector.h:254-258, 398-402`

`reset()` → `clean_up_elements()` は、`clean_up` が `nullptr` かどうかを確かめずに `(*clean_up)(element)` を呼びます。デストラクタ側では確認しているので、確認が抜けているのはこちらだけです。`undo_redo_vector<int>` に要素がある状態で `reset()` すると null 参照になります。アプリは `undo_redo_pointer_vector` だけを使っているため影響していませんが、汎用コンテナとしては欠陥です。

**修正案**: `clean_up_elements()` の先頭で `if (clean_up != nullptr)` を確認する。単体テストに `reset` のケースを追加する。

---

### M8. `transaction` と `undo_redo_collection` の例外安全性とコピー

**場所**: `undo_redo_vector.h:308-343, 191-215`

- `transaction::~transaction()` → `end_transaction()` は `std::logic_error` を投げることがあります。C++11 以降のデストラクタは暗黙に `noexcept` なので、投げると `std::terminate` になります。
- トランザクション中に例外が出ても、それまでのステップは確定扱いでグループに入ります（ロールバックしません）。
- トランザクションを入れ子にすると例外になります。たとえば `clear()` は内部でトランザクションを使うので、トランザクション中に `clear()` を呼ぶと失敗します。
- `undo_redo_collection` は生ポインタ（`clean_up`、`undo_steps`、`current_undo_step_group`）を所有していますが、コピーコンストラクタとコピー代入を禁止していません。コピーすると二重解放になります。
- 例外メッセージが `"an exception occurred"` だけで、原因が分かりません。
- `undo_data` は使われていません。

**修正案**:
- コピーを `= delete` にする。所有には `std::unique_ptr` を使う。
- トランザクションを入れ子に対応させる（深さカウンタを持つ）か、少なくともデストラクタでは例外を投げない。
- 使っていない `undo_data` を削除する。

---

### L1. 右クリックが左クリックと同じ扱いになる

**場所**: `MouseEventTranslator.h:106-113`

右ボタンを放したとき、ドラッグしていなければ `OnClick` が全リスナーに届きます。右ドラッグをパン（表示位置の移動）に使っているのに、右クリックだけだと選択の切り替えや図形の点の入力が起きます。`OnClick` にボタンの種類を渡し、`Command` 側で左ボタンだけを処理するのが自然です。

### L2. 描画領域の外でドラッグを終えると、状態が残る

**場所**: `Document.h:102-106`

`OnDragEnd` は `IsValid(point)` が偽だとコマンドに届きません。

- `SelectCommand` では `isAreaValid` が `true` のまま残り、範囲選択の枠が表示され続けます。
- 線や矩形のコマンドでは、ドラッグ開始で入力された 1 点目が残り、その後のクリックが 2 点目として扱われます。

ドラッグ終了点が領域外のときは、終了点を領域内に切り詰めるか、`OnDraggingAbort` として扱うのが安全です。

### L3. 何も選択していなくても Delete で変更フラグが立つ

**場所**: `Model.h:124-137`

`RemoveSelectedFigures` は、選択が空でも `NotifyObservers(Hint(Removed, {}))` を呼びます。そのため `Document::Update` で `SetModifiedFlag()` が実行されます。選択が空なら何もせずに戻るべきです。

### L4. 属性変更の `Hint` が古い図形を渡す

**場所**: `Model.h:84`

`Hint(Changed, selectedFigures)` で渡すのは、置き換える前の図形です。線幅を太くすると新しい図形の外接矩形のほうが大きくなり、`InvalidateRect` の範囲が足りません。現状は `OnMouseMove` で全体を再描画しているので目立ちませんが、`Model::Change` と同じように新旧両方を渡すのが正しい形です。

### L5. `RemoveObserver` が実際には削除しない

**場所**: `Observer.h:23-26`

`std::remove` は要素を後ろに寄せるだけです。`erase` と組み合わせて初めて削除されます。現在は呼び出し元がありませんが、H2 を直すときに必要になります。

### L6. デストラクタから通知が飛ぶ

**場所**: `Model.h:55-58`

`~Model()` → `Reset()` → `UnSelectAll()` → `SetSelectedFigureAttribute()` と進み、次の 2 つが起きます。

- `Application::Set`：すでに破棄されているかもしれないメインフレームに `SendMessage` する（`static_cast<MainFrame*>(nullptr)->GetSafeHwnd()` に依存）。
- `NotifyObservers`：破棄途中の `Document::Update` を呼ぶ。

デストラクタでは通知を伴わない後始末だけを行うべきです。

### L7. `TRACE` の書式と引数がずれている

**場所**: `MouseEventTranslator.h:46`

`TRACE(_T("(x: %d, y: %d)\n"), methodName, point.x, point.y);` は、`methodName` が余分です。`MOUSE_EVENT_TRANSLATOR_TEST` を有効にしたときだけ問題になります。

---

## 4. テスト

| ID | 内容 |
|---|---|
| T1 | `check_vector`（`Shos.MiniCadSample.Test.cpp:22`）の `for (; iterator != array.begin(); ...)` は、開始時点で条件が偽なので 1 回も回りません。`!= array.end()` の誤りと思われます。 |
| T2 | `MemoryLeakTest` の `main` は `undo_redo_pointer_vector_memory_leak_test()` を呼んでいません。アプリが実際に使っている `undo_redo_pointer_vector` のリーク検査が行われていません。 |
| T3 | `Assert::AreEqual(actual, expected)` の順で書かれています（正しくは期待値が先）。失敗時のメッセージが逆になります。 |
| T3 | `pointer_vector` と `clean_up` にはアサーションがありません。`clean_up` は `clean_up` なしの `undo_redo_vector<foo*>` を使うため、Undo ステップが持つ要素がリークします（テスト自体がリークする）。 |
| T3 | テストがない機能があります。`reset()`（M7）、入れ子のトランザクション、空のトランザクション、`update` の Undo と Redo の繰り返し、Redo 履歴を切り捨てたときの解放、`std::list` をコンテナにした場合です。 |
| T3 | `Model`、`Geometry`、`FigureAttribute::GetSum` は MFC への依存が比較的小さく、テストしやすい部分ですが、テストがありません。特に `Geometry` の距離計算（`GetDistanceToEllipse` は近似）や、H3 の属性マージは回帰テストを置く価値があります。 |

---

## 5. 設計・保守性に関するコメント（任意）

- **図形の所有と識別**: 図形をポインタで識別しているため、`Hint`、ハイライト、選択などで寿命の問題が起きやすくなっています（H4、L4）。サンプルの範囲を超えるかもしれませんが、図形に ID を持たせると、この種の問題をまとめて防げます。
- **選択状態が図形の中にある**: `Figure::isSelected` が `Clone()` でコピーされ、Undo ステップにも残ります。Undo すると選択状態まで巻き戻るのは意図どおりかもしれませんが、選択は View 側の状態として Model の外に置く設計も考えられます。
- **描画キャッシュの粒度**: `View::OnUpdate` は差分を `InvalidateRect` していますが、その直前の `Update()` で Layer1 のキャッシュ全体を作り直しています。そのため差分再描画の効果は BitBlt の範囲に限られます。また `OnMouseMove` が毎回 `Invalidate()` で全体を再描画しています。サンプルの規模では問題ありませんが、差分描画を見せる教材としては説明があるとよいです。（9 章の更新で、キャッシュに差分だけを描き足す `CacheDevice` が使えるようになりました。ただし View はまだ使っていません）
- **使われていないコード**: ~~`DoubleBuffer::CashDevice`~~（9 章で `CacheDevice` として整備され、公開 API になりました）、`undo_data`、`Model::Remove` / `Change`、`Document::Add`、`View::OnDragging`（コメントアウト）、`Zooming::OnDragging`（コメントアウト）、`Zooming::ShiftTo` 内の未使用変数 `newLogicalArea`。
- **乱数**: `FigureHelper::RandomValue` は `mt() % n` です。`std::uniform_int_distribution` を使うほうが偏りがなく、意図も明確です。`GetRandomFigure` の `default` 分岐は `nullptr` を参照外しします（到達しない分岐ではあります）。
- **ヒットテスト**: `Geometry::InRect` は `PtInRect` を使っています。右端と下端を含まないため、範囲選択の枠にぴったり接している図形は選択されません。
- **コーディングの一貫性**:
  - `override` が付いているメソッドと付いていないメソッドが混在しています（`GetName`、`GetPoints`、`GetMaximumCount` など）。
  - `DotFigure::radius` は非 static の `const` メンバです（`static constexpr` が適当）。
  - `Zooming` のメンバ初期化子の順序が宣言順と違います（C5038 警告の対象）。
- **綴り**: `Hilight` → `Highlight`、`DrawSelecter` → `DrawSelector`、~~`Cash` → `Cache`~~（9 章で修正済み）、`erace` → `erase`（テスト名）、`maxmum` → `maximum`、`searchinRect` → `searchingRect`。
- **README**: 機能一覧のインデントにタブとスペースが混在しており、GitHub で入れ子が崩れて表示されます。ビルド方法とテストの実行方法の記述もありません（`CLAUDE.md` には記載済み）。

---

## 6. 良い点

- Model、Document、View、Command の責務がはっきり分かれており、`Hint` による差分通知の流れが追いやすいです。
- `MouseEventTranslator` でマウス操作を意味のあるイベントに変換し、クリック入力とドラッグ入力を同じコマンドで受け付けられる設計は、再利用しやすい部品です。
- `undo_redo_vector` は MFC に依存しない汎用コンテナとして分離されており、単体テストとメモリリーク検査のプロジェクトも用意されています。
- `GdiObjectSelector` / `StockObjectSelector` による RAII で、GDI オブジェクトの選択を確実に元へ戻しています。
- クリップボードへのコピーで、拡張メタファイル、ビットマップ、独自形式の 3 種類を同時に出力しており、教材として価値があります。

---

## 7. 推奨する対応順

1. **H2**（重複登録）: 修正は小さく、効果が大きい。
2. **H1**（Cut）と **M2**（Paste）: クリップボード操作を Model の、Undo できる API に寄せる。
3. **H4**（ハイライトのポインタ）と **M6**（`Model::Remove`）: メモリ安全性。
4. **H3**（属性のマージ）: 回帰テストを書いてから直す。
5. **M1**、**M3**、**M4**、**M5**。
6. テスト（T1〜T3）の整備。M7 と M8 は `undo_redo_vector.h` の更新で解消済みです（8 章）。
7. 低優先度の項目と保守性の改善。

---

## 8. 追記: `undo_redo_vector.h` の更新の反映（2026-11-08）

作業ツリー上で `undo_redo_vector.h` が全面的に書き直されました（未コミット、419 行 → 774 行）。新しい版を通読し、利用側（`Model.h`、単体テスト、メモリリーク検査）との整合を確認しました。こちらもビルドはしていません。

### 8.1 主な変更点

- 各操作を表すクラスを `insert_or_remove_step`、`update_step`、`group_step` に分け、`std::unique_ptr` で所有するようにした。
- `clean_up` を `std::function<void(TElement&)>` として値で持つようにした。空なら何も呼ばない。
- コピーを禁止し、ムーブに対応した。
- トランザクションを入れ子に対応させた（深さカウンタ）。`end_transaction` を `noexcept` にし、メモリ確保を `begin_transaction` / `execute` で前もって済ませるようにした。
- トランザクション中は `undo` / `redo` が `false` を返し、`reset` は `std::logic_error` を投げる。
- `iterator` を `const_iterator` の別名にし、要素を直接書き換えられないようにした（履歴を経由しない変更の防止）。
- `insert`、`at`、`front`、`back`、`empty`、`in_transaction` を追加した。
- スレッドセーフなラッパー `synchronized_undo_redo_collection` / `synchronized_undo_redo_vector` を追加した。
- 使われていなかった `undo_data` を削除した。

### 8.2 既存の指摘への影響

| ID | 状態 | 根拠 |
|---|---|---|
| M7 | **解消** | `clean_up_all()` が `if (clean_up)` を確認するようになった（`undo_redo_vector.h:546`）。 |
| M8 | **解消** | コピー禁止（284-285 行）、入れ子トランザクション（478-499 行）、`end_transaction() noexcept`（490 行）。例外で抜けたときに途中までの変更を 1 ステップとして残す動作は、仕様としてコメントに明記された（258 行）。 |
| M1 | **未解消** | 履歴だけを消して要素を残す API（計画書の `clear_history()`）はまだない。`reset()` は要素も解放する。 |
| T1〜T3 | **未解消** | テストは更新されていない。新しく追加された機能にもテストがない。 |

利用側との互換性:
- `Model.h` が使っているのは `begin`/`end`/`cbegin`/`cend`、`push_back`、`erase`、`update`、`undo`/`redo`、`can_undo`/`can_redo`、`reset`、`size`、`transaction` だけです。いずれもシグネチャに互換性があります。`std::find(figures.begin(), ...)` の結果は `const_iterator` になりますが、`erase` / `update` が `const_iterator` を受け取るので問題ありません。
- `Model::Select` などは、`const_iterator` 経由で `Figure*` が指す先（`Figure`）を書き換えています。ポインタそのものは `const` ですが、指す先は変更できるのでコンパイルは通ります。ただし、選択状態の変更は Undo 履歴を経由しない、という設計上の性質は変わりません（5 章）。
- メモリリーク検査の `undo_redo_vector<foo*> array([](foo* foo) { delete foo; })` は、`foo*` を値で受け取るラムダから `std::function<void(foo*&)>` への変換になります。これは有効です。

### 8.3 新しい版に対する指摘

| ID | 重大度 | 場所 | 概要 |
|---|---|---|---|
| N1 | 低 | `undo_redo_vector.h:215, 482, 511` | 確保を `reserve(size + 1)` で 1 つずつ増やしている。MSVC STL の `reserve` は要求どおりの容量を確保し、倍々には増やさない。そのため Undo 履歴への追加や大きなトランザクション（`clear()` など）のたびに再確保とムーブが起き、全体の計算量が O(n²) になる。 |
| N2 | 低 | `undo_redo_vector.h:504-515` | `execute` で `step->redo(data)` が例外を投げると（`insert` 時のメモリ確保失敗など）、ステップが持っていた新しい要素は `clean_up` を通らずに破棄される。`undo_redo_pointer_collection` ではその `Figure` がリークする。 |
| N3 | 低 | `undo_redo_vector.h:519-549` | `discard_undo_steps` と `clean_up_all` は `noexcept` だが、利用者が渡した `clean_up` を呼ぶ。`clean_up` が例外を投げると `std::terminate` になる。ヘッダーのコメントで「`clean_up` は例外を投げてはならない」と明記するとよい。 |
| N4 | 情報 | `undo_redo_vector.h:565-761` | `synchronized_undo_redo_collection` はアプリでは使われておらず、テストもない。残すのであれば単体テストを追加する。 |

**N1 の修正案**: 容量が足りないときだけ倍々に増やす補助関数を使う。

```cpp
template <typename TVector>
static void reserve_for_one_more(TVector& vector, std::size_t required)
{
    if (vector.capacity() < required)
        vector.reserve(std::max(required, vector.capacity() * 2));
}
```

**N2 の修正案**: `execute` で `redo` を `try` で囲む。失敗したら `clean_up` があれば `step->clean_up_elements(clean_up)` を呼んでから、例外を投げ直す。

---

## 9. 追記: ダブルバッファ関連の更新の反映（2026-11-08）

作業ツリー上で、次の 4 ファイルが更新されました（未コミット）。新しい版を通読し、利用側との整合を確認しました。こちらもビルドはしていません。

- `DoubleBuffer.h`（全面改訂）
- `DoubleBuffer.cpp`
- `GdiObjectSelector.h`
- `View.h`

### 9.1 主な変更点

- **Layer 2 も画面外で合成するようにした**: 以前は Layer 1（キャッシュ）を画面に転送してから、Layer 2（ラバーバンドやカーソル表示）を画面に直接描いていました。今回の版では、再描画範囲の大きさの作業用ビットマップにキャッシュをコピーし、そこに Layer 2 を描いてから、画面へ一度だけ転送します。Layer 2 のちらつきがなくなります。
- **スクロールの扱いを変えた**: `OnHScroll` / `OnVScroll` / `OnMouseWheel` の個別処理をやめ、`CScrollView::OnScrollBy` のオーバーライドにまとめました。キャッシュを作ったときのスクロール位置（`cacheOrigin`）を記録し、位置が変わっていたら作り直します。
- **`CashDevice` を `CacheDevice` に改名した**: キャッシュに差分だけを描き足すための API として整備しました。`OnPrepareDC` を適用するので論理座標で描け、コピーは禁止です。旧名は `[[deprecated]]` の別名として残っています。
- **その他**:
  - 名前空間 `Shos::MFCDoubleBuffer` を導入した。
  - テンプレートを C++20 のコンセプト `std::derived_from<CView>` で制約した。
  - 描画の入口になる仮想関数を `OnPaint(CDC&)` から `OnPaintBuffered` に改名した（`afx_msg OnPaint()` と同じ名前で紛らわしかったため）。
  - `SetBackgroundColor` でキャッシュを無効にするようにした。
  - 最小化時（クライアント領域が空）は描画しないようにした。
  - ビットマップの作成に失敗したときの戻り値を確認するようにした。
- **`GdiObjectSelectorBase` のコピーを禁止した**: コピーすると選択が二重に元へ戻されるのを防ぎます。
- **`View.h`**: `using namespace Shos::MFCDoubleBuffer;` を追加しました。

### 9.2 既存の指摘・コメントへの影響

| 項目 | 状態 |
|---|---|
| 5 章「使われていないコード」の `CashDevice` | **方針変更**: 削除ではなく、`CacheDevice` という公開 API として整備された。アプリからはまだ使われていない。 |
| 5 章「綴り」の `Cash` → `Cache` | **解消** |
| 5 章「描画キャッシュの粒度」 | **一部前進**: 差分を描き足す手段（`CacheDevice`）はできた。ただし `View::OnUpdate` は今も毎回 `Update()` でキャッシュ全体を作り直している。 |
| H1〜H4、M1〜M6、L1〜L7 | 影響なし（いずれも残っている）。 |

利用側との互換性:
- `View.h` / `View.cpp` は、追加された `using namespace` によって `DoubleBufferView` / `DoubleBufferScrollView` を修飾なしで参照できます。`IMPLEMENT_DYNCREATE(View, DoubleBufferView)` と `BEGIN_MESSAGE_MAP(View, DoubleBufferView)` も、`View.h` を読み込んだ後なので解決できます。
- `View` が使う `Update()`、`SetBackgroundColor()`、`OnDrawLayer1/2`、`OnUpdate`、`OnPrepareDC`、`OnMouseWheel` は、いずれも名前と可視性に互換性があります。改名された `OnPaint(CDC&)` を使っている箇所はありません。
- Layer 2 の描画先が、画面の DC から作業用ビットマップの DC に変わりました。原点は `OffsetViewportOrg(-clipBox.left, -clipBox.top)` で補正されています。Layer 2 の中で座標変換を使う箇所（`Cursor::DrawCursor` の `LPtoDP` / `DPtoLP`、`Geometry::DPtoLP(dc, size)`）や、XOR 描画（`SelectCommand::DrawArea` の `R2_XORPEN`）も、同じ DC 上で完結するので問題ありません。

### 9.3 新しい版に対する指摘

| ID | 重大度 | 場所 | 概要 |
|---|---|---|---|
| D1 | 低 | `View.h:13` | `using namespace Shos::MFCDoubleBuffer;` がヘッダーの**グローバル スコープ**にあり、`View.h` を読み込むすべての翻訳単位（`Application.cpp` など）に名前空間が漏れる。`View.cpp` で `using` するか、`View.h` では `Shos::MFCDoubleBuffer::DoubleBufferView` のように修飾するほうがよい。 |
| D2 | 低 | `DoubleBuffer.h:144-151` | WM_PAINT のたびに、再描画範囲の大きさの作業用ビットマップを作って破棄している。`View::OnMouseMove` が毎回 `Invalidate()` で全体を再描画するため、マウスが動くたびにクライアント領域全体の大きさのビットマップを作り直すことになる。作業用ビットマップもメンバーに持ち、大きさが変わったときだけ作り直すと負荷が下がる。 |
| D3 | 低 | `DoubleBuffer.h:137-150` | キャッシュや作業用ビットマップを作れなかったときは何も描かずに戻る。`OnEraseBkgnd` が背景を消さないので、再描画範囲に以前の画面が残ったままになる。最低限、背景色で塗りつぶして Layer 1 と Layer 2 を画面に直接描くといった代替処理があるとよい。 |
| D4 | 低 | `DoubleBuffer.h:74-83, 97-98` | `CacheDevice` を最初の WM_PAINT より前（キャッシュのビットマップがまだない状態）に使うと、空の `CBitmap` を選択しようとして選択が失敗し、描いた内容は黙って捨てられる。コメントには「キャッシュが無効なとき」とあるが、この場合も含まれることを明記するか、`ASSERT(cache.GetSafeHandle() != nullptr)` を置くとよい。また旧名の別名 `CashDevice` は利用箇所がないので削除できる。 |
| D5 | 低 | `DoubleBuffer.h:1, 215`, `DoubleBuffer.cpp:1-2` | コメントのファイル名が `MFCDoubleBuffer.h` / `MFCDoubleBuffer.cpp` になっているが、実際のファイル名は `DoubleBuffer.h` / `DoubleBuffer.cpp`。 |
| D6 | 情報 | リポジトリ全体 | 名前空間の命名が `shos`（`undo_redo_vector.h`、小文字）と `Shos::MFCDoubleBuffer`（今回、パスカルケース）で揃っていない。アプリ本体のクラスはグローバル名前空間にある。方針を決めて統一するとよい。 |

### 9.4 良い点

- Layer 2 を画面外で合成するようにしたことで、ラバーバンドやカーソル表示のちらつきが根本的になくなります。
- スクロールの処理を `OnScrollBy` 1 か所に集めたので、スクロール バー、ホイール、キーボードなど、どの経路でスクロールしてもキャッシュの扱いが一貫します。
- ヘッダー冒頭に、仕組み、使い方、制限事項が日本語でまとまっており、教材としての価値が上がっています。
