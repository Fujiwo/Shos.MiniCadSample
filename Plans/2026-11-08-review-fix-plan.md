# レビュー指摘の修正計画

- 作成日: 2026-11-08
- 元にしたレビュー: [`Reviews/2026-11-08-repository-review.md`](../Reviews/2026-11-08-repository-review.md)
- 対象コミット: `ae5c882`（main）
- 更新: 2026-11-08 — 作業ツリー上の `undo_redo_vector.h` の全面改訂を反映しました。M7 と M8 は解消済みなので、フェーズ 1 を縮小し、新しい指摘 N1〜N4 を加えました（レビュー 8 章）。
- 更新: 2026-11-08 — `DoubleBuffer.h/.cpp`、`GdiObjectSelector.h`、`View.h` の更新を反映しました。フェーズ 7 を見直し、新しい指摘 D1〜D6 を加えました（レビュー 9 章）。
- 更新: 2026-11-08 — 計画レビューを反映し、クリップボード描画の座標変換と Paste の例外安全性、フェーズ間の依存関係、ベースラインとビルド構成を明確にしました。

---

## 1. 目的と方針

レビューで挙がった指摘（H1〜H4、M1〜M8、L1〜L7、T1〜T3）を、依存関係に沿った順番で直します。

基本方針:

- **土台から直す**: 多くの修正は、`undo_redo_vector` と `Model` に新しい API（履歴だけを消す、まとめて貼り付ける、など）を足すことが前提になります。そこで、基盤 → Model → 利用側（クリップボード、ダイアログ、View）の順に進めます。
- **ファイル形式は変えない**: `Figure` 系のシリアライズ形式（スキーマ 1）はそのままにし、既存の保存ファイルを開けるようにします。クリップボードの独自形式だけは変わります（M4）。
- **直す前にテストを書く**: MFC に依存しない部分（`undo_redo_vector`、`FigureAttribute`、`Geometry`）は、修正の前に失敗するテストを書きます。
- **小さく分けてコミットする**: 作業ブランチ `fix/review-2026-11-08` を切り、原則として指摘 ID ごとに 1 コミットにします。コミットメッセージには指摘 ID（例: `[H2]`）を入れます。
- **大きな再設計はしない**: 図形を ID で識別する設計や、選択状態を Model の外に出す設計は、今回は扱いません（8 章を参照）。

---

## 2. フェーズ一覧

| フェーズ | 内容 | 対象 ID | 前提 |
|---|---|---|---|
| 0 | 準備（ベースラインの確認） | ― | ― |
| 1 | `undo_redo_vector` の改訂版の確定とテストの整備 | M1 の前提 API、N1〜N4、T1、T2、T3（一部）（M7 と M8 は解消済み） | 0 |
| 2 | 登録の重複をなくす | H2, L5 | 0 |
| 3 | Model の一貫性 | H4, M1, M6, L3, L4, L6 | 1 |
| 4 | クリップボード | H1, M2, M4 | 3 |
| 5 | 図形属性の受け渡し | H3, T3（一部） | 0, 3 |
| 6 | 入力・ズーム・ダイアログ | M3, M5, L1, L2, L7 | 0 |
| 7 | 保守性の改善（任意） | 5 章の項目、D1〜D6 | 1〜6 |

フェーズ 2 と 3 は、対象ファイルが重ならないため、別ブランチまたは worktree で並行作業できます。フェーズ 4、5、6 はフェーズ 3 完了後に進めます。これらは `Model.h`、`View`、`Document`、`MouseEventTranslator.h` で変更対象が重なるため、同じ作業ブランチ上で並行実装しません。フェーズ 7 は他の変更が落ち着いてから行います。

---

## 3. 各フェーズの詳細

### フェーズ 0: 準備

1. `git status` と差分を確認し、`undo_redo_vector.h`、ダブルバッファ関連ファイルなど、計画が前提とする既存変更を特定して保存します。これらを失ったり、無関係な変更と混ぜたりしないでください。前提変更が未コミットなら、内容を確認して独立したコミットにするか、その変更を含むコミットから作業ブランチを作成します。作業ブランチが前提変更を含むことを確認してから着手します。
2. `fix/review-2026-11-08` を、前提変更を含むベースから作成します。対象コミット `ae5c882` だけをベースにして前提変更を欠落させないでください。
3. Windows 上で `Debug` / `Release` × `x64` / `Win32` の全構成をビルドし、警告の数を記録します。後で比べるためです。
4. `Shos.MiniCadSample.Test` を実行し、現状の結果を記録します。
5. `Shos.MiniCadSample.MemoryLeakTest` を Debug で実行し、出力ウィンドウにリーク報告があるか記録します。
6. レビューで「要実機確認」とした項目を、実際に動かして確かめます。
   - **M2**: 貼り付けた図形がすぐに表示されないか。
   - **M5**: × ボタンや Esc でダイアログが閉じないか。
   - **H2**: 新規作成を 2 回した後、Dot で 1 回クリックすると点が 2 つ増えるか。

   再現しなくても直ちに計画から外しません。再現条件、実行した構成、コード上の根拠を確認し、指摘の前提が現行コードで成立しないと確認できた場合に限り、理由を記録して対象外または優先度変更とします。静的に確認された問題は、手動で再現しないだけでは除外しません。

---

### フェーズ 1: `undo_redo_vector` の改訂版の確定とテストの整備

**対象ファイル**: `Shos.MiniCadSample/undo_redo_vector.h`、`Shos.MiniCadSample.Test/Shos.MiniCadSample.Test.cpp`、`Shos.MiniCadSample.MemoryLeakTest/Shos.MiniCadSample.MemoryLeakTest.cpp`

> **現状**: `undo_redo_vector.h` は作業ツリー上で全面的に改訂されており（未コミット）、M7（`reset()` の null 参照）と M8（コピー禁止、入れ子トランザクション、`noexcept` のデストラクタ、`undo_data` の削除）は解消済みです。このフェーズでは、改訂版をビルドとテストで確かめてコミットし、残っている課題を片付けます。

#### 1-0. ダブルバッファ関連の更新の確認（レビュー 9 章）
`DoubleBuffer.h/.cpp`、`GdiObjectSelector.h`、`View.h` の更新も、ここで同時に確かめます。
- 全構成でビルドし、`View.cpp` の `IMPLEMENT_DYNCREATE` / `BEGIN_MESSAGE_MAP` が名前空間 `Shos::MFCDoubleBuffer` の基底クラスを正しく解決すること。
- 手動で次を確かめます。
  - 範囲選択の枠、図形入力中のプレビュー、カーソル表示がちらつかずに描かれる。
  - ウィンドウのサイズ変更、最小化からの復元、Ctrl+ホイールのズーム、右ドラッグのパンで、表示が崩れない。
  - `View.h` の `SCROLL_VIEW` を有効にしてビルドし、スクロール バー、ホイール、キーボードでスクロールしても表示が崩れない。

#### 1-1. 改訂版のビルドと互換性の確認
- 全構成（`Debug`/`Release` × `x64`/`Win32`）で 3 プロジェクトをビルドします。利用側は静的に確認したかぎり互換性があります（レビュー 8.2）が、次の点はコンパイラで確かめます。
  - `Model.h` の `std::find(figures.begin(), ...)` → `erase` / `update` が、`const_iterator` で通ること。
  - `MemoryLeakTest.h` の `#define new DEBUG_NEW` と、新しく読み込まれる `<memory>`、`<optional>`、`<shared_mutex>` が衝突しないこと。
  - 新たに出る警告（`[[nodiscard]]` の戻り値を使っていない箇所など）がないこと。
- 既存の単体テストが成功し、アプリの Undo/Redo、削除、属性変更が従来どおり動くことを確かめたら、この時点で改訂版を単独でコミットします。

#### 1-2. [M1 の前提] 履歴だけを消す API の追加
改訂版にも、要素を残して履歴だけを捨てる API はありません。次を追加します。

```cpp
// Discards the undo history but keeps the current elements.
// Elements held only by the history are cleaned up. Throws std::logic_error during a transaction.
void clear_history()
{
    if (in_transaction())
        throw std::logic_error("undo_redo_collection::clear_history: cannot clear the history during a transaction");
    discard_undo_steps(0);
    undo_steps_index = 0;
}
```

`synchronized_undo_redo_collection` にも、ロックを取って委譲する同名のメソッドを追加します。

#### 1-3. [N1] 確保量を倍々に増やす
- `reserve(size + 1)` の 3 か所（`group_step::reserve_one_more`、`begin_transaction`、`execute`）を、容量が足りないときだけ `max(required, capacity * 2)` を確保する補助関数に置き換えます。例外を投げないことの保証（`end_transaction` と `push_to_undo_steps` の中では確保しない）はそのまま守られます。
- 確認: 1 万要素の `clear()` と、1 万回の `push_back` が実用的な時間で終わること（単体テストに入れるか、手動で測ります）。

#### 1-4. [N2][N3] 例外時の後始末と契約の明記
- `execute` で `step->redo(data)` を `try` で囲みます。例外が出たら `clean_up` があれば `step->clean_up_elements(clean_up)` を呼び、例外を投げ直します。`clean_up` は例外を投げない契約とし、失敗時に要素がコレクションとステップのどちらに所有されているかを各 `redo` 操作で確認します。
- 例外注入テストで、`redo` 失敗時に要素がリークも二重解放もせず、コレクションと Undo 履歴が一貫した状態に保たれることを確認します。トランザクション中に途中まで成功した操作を保持する既存仕様と矛盾しないことも確認します。
- ヘッダー冒頭のコメントに「`clean_up` は例外を投げてはならない」と追記します（N3）。

#### 1-5. [T1][T3] 単体テストの修正と追加
- 既存テストの修正:
  - `check_vector` のループ条件を `!= array.end()` に直す。
  - `Assert::AreEqual` の引数を（期待値, 実際値）の順に揃える。
  - `clean_up` テストに `clean_up` を渡し、テスト自体がリークしないようにする。
  - `pointer_vector` テストに、要素数と値のアサーションを追加する。
- 解放を確かめるため、「生存数を数える」クラス（コンストラクタで増やし、デストラクタで減らす）を用意し、`clean_up` がちょうど 1 回ずつ呼ばれることを確かめます。
- 改訂版で追加・変更された動作のテストを追加します。

  | 分類 | テストケース |
  |---|---|
  | 要素アクセス | `insert`、`at`（範囲外で `std::out_of_range`）、`front`、`back`、`empty` |
  | トランザクション | 入れ子（外側の終わりで 1 ステップになる）、空のトランザクション（ステップが増えない）、トランザクション中は `undo`/`redo` が `false`、`reset` が `std::logic_error`、例外で抜けても途中までの変更が 1 ステップとして残る |
  | 履歴 | `update` の Undo と Redo の繰り返し、Redo 履歴の切り捨て時の `clean_up`、`clear_history()`（1-2） |
  | 所有 | ムーブ構築とムーブ代入（移動元が空になり、二重に解放されない）、`reset()`（`clean_up` なしでも落ちない） |
  | コンテナ | `std::list` をコンテナにした基本操作、`std::unique_ptr` のようなムーブ専用型 |
  | スレッドセーフ版（N4） | `synchronized_undo_redo_vector` の基本操作、`modify` が 1 ステップになること、複数スレッドから同時に `push_back` しても要素数が合うこと |

  N4 で、スレッドセーフ版を残さない（削除する）と決めた場合は、最後の行は不要です。

#### 1-6. [T2] メモリリーク検査
- `main` から `undo_redo_pointer_vector_memory_leak_test()` を呼びます。
- 入れ子トランザクション、`clear_history()`、`reset()`、ムーブ代入の利用例も追加します。

**完了条件**: 全構成でビルドが通り、単体テストがすべて成功し、`MemoryLeakTest` を Debug で実行してリーク報告が出ないこと。

---

### フェーズ 2: 登録の重複をなくす

**対象ファイル**: `Observer.h`、`MouseEventTranslator.h`、`View.h`

#### 2-1. [L5] `RemoveObserver` を正しく実装する
```cpp
auto it = std::remove(observers.begin(), observers.end(), &observer);
bool removed = it != observers.end();
observers.erase(it, observers.end());
return removed;
```

#### 2-2. [H2] 登録を冪等にする（同じ相手を何度登録しても 1 回分になるようにする）
- `Observable::AddObserver` と `MouseEventTranslator::AddListener` で、すでに登録済みなら追加しないようにします（`std::find` で確認）。
- 加えて `View` に `bool isInitialized` を持たせ、`OnInitialUpdate` の中のリスナー登録とオブザーバー登録（`SetFigureAttributeObserver`、`AddListener` 3 か所）は初回だけ行います。`Application::Set(...)` による属性の表示更新は毎回行います。
- SDI では `Document` オブジェクトが使い回されるため、登録先のポインタは変わりません。したがって初回だけ登録すれば足ります。

**完了条件（手動）**: 新規作成を 3 回した後でも、Dot で 1 回クリックすると点が 1 つだけ増えること。ダイアログで線幅を 1 回変えた後、Ctrl+Z を 1 回押すと元に戻ること。

---

### フェーズ 3: Model の一貫性

**対象ファイル**: `Model.h`、`Document.h`、`Application.h`

#### 3-1. [H4] ハイライトのポインタを無効化する
- `Model` に private な `InvalidateHighlight()`（`highlightedFigure = nullptr`）を追加します。
- 図形の集合を変える操作では必ず呼びます。対象は `Undo`、`Redo`、`RemoveSelectedFigures`、`Update`（属性変更）、`Add`、`AddDummyData`、`Reset`、および新しく追加する `Paste` と `Load` です。
- 次にマウスが動いたときに `SelectCommand::OnCursorMove` がハイライトを設定し直すので、操作性への影響はありません。

#### 3-2. [M6] 使われていない、壊れたメソッドを削除する
- `Model::Remove(Figure*)`、`Model::Change(Figure*, Figure*)`、`Document::Add(Figure*)` は呼び出し元がないので削除します。
- 将来必要になったときは、`erase` の前に要素を退避する形で作り直します。

#### 3-3. [L3] 空の削除を無視する
- `RemoveSelectedFigures` で選択が空なら、何もせずに戻ります（通知もしない）。

#### 3-4. [L4] 属性変更の通知に新旧両方の図形を含める
- `Model::Update(selectedFigures, attribute)` で、作った複製（新しい図形）を集め、`Hint(Changed, 旧図形 + 新図形)` で通知します。

#### 3-5. [M1] ファイル読み込みと履歴を切り離す
- `Model::Serialize` の読み込み処理の最後で `figures.clear_history()` を呼び、`Hint::Type::All` を通知します。
- 読み込み中に例外が出た場合も、途中まで読み込んだ図形は `DeleteContents` で破棄されるので、現状と同じ扱いで問題ありません。
- クリップボードからの読み込みは、フェーズ 4 で別の API（`Paste`）に分けます。`Serialize` はファイル専用にします。

#### 3-6. [L6] デストラクタから通知しない
- `Reset()` を「内部の後始末」と「通知」に分けます。

  ```cpp
  void Reset()        { Clear(); SetSelectedFigureAttribute(); }  // 通知あり（DeleteContents から呼ぶ）
  virtual ~Model()    { Clear(); }                                // 通知なし
  private: void Clear() { figures.reset(); highlightedFigure = nullptr; }
  ```

- `Application::Set` と `SetFigureAttributeObserver` では、`AfxGetMainWnd()` が `nullptr` のときは何もしないようにし、`static_cast` の前に確認します。

**完了条件（手動）**:
- ファイルを開いた直後は Undo が無効であること。
- レビュー H4 の再現手順（ハイライト → Ctrl+Z → キーボードで Figure → Random）でクラッシュせず、図形のない場所に枠が残らないこと。
- 何も選択せずに Delete を押しても、タイトルの変更マーク（`*`）が付かないこと。

---

### フェーズ 4: クリップボード

**対象ファイル**: `ClipboardHelper.h/.cpp`、`Model.h`、`Document.h`、`View.h/.cpp`

#### 4-1. 仕様の決定（変更前に確定する）

| 操作 | 選択あり | 選択なし |
|---|---|---|
| Copy | 選択図形を独自形式と画像（EMF、DIB）で出力 | 全図形を出力（現状と同じ） |
| Cut | 選択図形をコピーし、Undo できる操作として削除 | **無効**（メニューをグレーアウト） |
| Paste | 1 回の Undo で取り消せる単位で追加し、貼り付けた図形を選択状態にする | 同じ |

選択なしの Copy は全図形を出力する仕様に固定します。画像として図全体をコピーする現在の使い方を維持するためです。

#### 4-2. Model への API 追加
- `void StoreFigures(CArchive& ar, bool selectedOnly) const`: 対象の図形を書き出します（件数 + `WriteObject`）。
- 画像出力は対象図形の外接矩形を出力範囲にします。選択図形を描く場合は、その外接矩形の左上を出力先の原点に移す座標変換とクリッピングを適用します。描画コールバックまたは `DrawFigures` の引数で出力範囲を渡し、単に画像サイズだけを外接矩形に変更してモデル座標のまま描かないようにします。全図形を出力する場合は従来のドキュメント範囲を維持します。
- `void Paste(CArchive& ar)`: 次の順に処理します。
  1. 入力全体を一時領域へ読み込み、形式と件数を検証する。まだ Model は変更しない。
  2. 読み込んだ図形を RAII で一時所有し、読み込み失敗時に全て解放できるようにする。
  3. 追加に必要な領域を確保してから既存の選択を解除し、トランザクション内で追加する。追加途中で例外が起きても部分貼り付けを残さないよう、事前確保でコミット中の失敗を防ぐか、変更済み要素を安全に巻き戻す仕組みを用意する。現行のトランザクションは例外時も途中までの変更を保持するため、RAII トランザクションだけに原子性を依存させない。
  4. 成功時に一時所有権を Model に移し、貼り付けた図形を選択状態にする。
  5. `Hint(Added, 追加した図形)` を一度だけ通知する。

  これで M2（表示されない、Undo が 1 図形ずつになる）を解決します。
- Paste の失敗テストを追加します。アーカイブが途中で壊れている場合は Model が変更されず、一時読み込み済み図形もリークしないこと、追加時の例外注入でも部分貼り付けや二重解放が起きないことを確認します。

#### 4-3. [H1] Cut を作り直す
- `View::OnEditCut` を「Copy（選択図形） + `Model::RemoveSelectedFigures()`」にします。`DeleteContents()` は呼びません。
- 変更フラグは、`Document::Update` が通知を受けて自動で立てます。

#### 4-4. [M4] 独自のクリップボード形式を登録する
- `static UINT GetFormat()` で `::RegisterClipboardFormat(_T("Shos.MiniCadSample.Figures"))` を一度だけ呼び、その値を使います。
- 登録した形式のデータはシステムが解放するため、`globalMemoryHandle` と `OnDestroyClipboard`（`ON_WM_DESTROYCLIPBOARD` を含む）を削除します。
- 読み込みは `try { ... } catch (CException* e) { e->Delete(); return false; }` で囲みます。

#### 4-5. `ClipboardHelper` のインターフェース整理
- `CDocument&` を受け取る代わりに、書き出し、描画、読み込みの処理を `std::function` で受け取る形にし、`ClipboardHelper` を Model の詳細から切り離します。

  ```cpp
  static bool Copy (CWnd& window, CSize size, COLORREF background,
                    std::function<void(CArchive&)> store, std::function<void(CDC&)> draw);
  static bool Paste(CWnd& window, std::function<void(CArchive&)> load);
  static bool CanPaste();
  ```

- 画像のサイズは、選択図形の外接矩形にします。全図形を出力するときは、現状どおりドキュメントのサイズです。
- 受け入れ確認に、ドキュメント原点から離れた位置にある図形の選択コピーを追加します。独自形式の貼り付け、EMF/DIB の貼り付けで図形全体が表示され、原点位置や不要な余白が誤らないことを確認します。

#### 4-6. コマンド UI
- `ON_UPDATE_COMMAND_UI` を追加します。
  - Cut: 選択があるときだけ有効
  - Copy: 4-1 で決めた仕様に従う
  - Paste: `IsClipboardFormatAvailable(GetFormat())` が真のときだけ有効

**完了条件（手動）**:
- 図形を 1 つ選んで Ctrl+X を押すと、その図形だけが消える。Ctrl+Z で戻る。
- Ctrl+V で貼り付けた図形がすぐに表示され、Ctrl+Z を 1 回押すと貼り付け全体が取り消される。
- 2 つのインスタンスを起動し、一方でコピーしたものを他方に貼り付けられる。
- Word などに Ctrl+V すると図（EMF）として貼り付けられる。

---

### フェーズ 5: 図形属性の受け渡し

**対象ファイル**: `FigureAttribute.h`、`FigureAttributeDialog.h/.cpp`、`Model.h`、テストプロジェクト

#### 5-1. [H3] 変更した項目だけを適用する
- `FigureAttribute` を次のように変えます。
  - `SetColor` / `SetPenWidth` で、対応する有効フラグを `true` にする。
  - 有効な項目だけを `target` に上書きする `void ApplyTo(FigureAttribute& target) const` を追加する。
  - 変更の差分を表す、全項目が無効な属性を作る `static FigureAttribute Empty()` を追加する。
- ダイアログは、変更された項目だけが有効な「差分」の属性を作って通知します。
  - `SaveColor`: `auto change = FigureAttribute::Empty(); change.SetColor(...); NotifyObservers(change);`
  - `SavePenWidth`: 同じ形にする。
  - ダイアログ自身が表示用に持つ `figureAttribute` も、同じ差分で更新します。
- `Model::Update(const FigureAttribute& change)` では、次のように差分を適用します。
  - 選択なし: `change.ApplyTo(currentFigureAttribute)`
  - 選択あり: 各複製に `change.ApplyTo(updatedFigure->Attribute())`
- 図形が持つ属性の有効フラグは、常に `true` のままになります。

#### 5-2. 色と線幅を同時に変えたときの扱い
- 「Set」ボタン 1 回で色と線幅の両方が変わった場合は、通知を 1 回にまとめて Undo ステップが 1 つになるようにします。`Save` の中で差分を 1 つにまとめてから、1 回だけ通知します。

#### 5-3. [T3] テスト
- テストプロジェクトから `FigureAttribute` と `Geometry` を使えるようにします。どちらも `<afx.h>` に依存しているため、次のどちらかを選びます。
  - **案 A（推奨）**: テストプロジェクトの `UseOfMfc` を `Dynamic` にし、`pch.h` で `<afx.h>` を読み込む。変更が小さく済みます。
  - **案 B**: MFC に依存しない部分を別のヘッダーに切り出す。変更が大きいので今回は見送ります。
- テストケースを追加します。
  - `GetSum`: 値が混在する場合と一致する場合
  - `ApplyTo`: 有効な項目だけが上書きされること
  - H3 の再現（線幅 1 と 3 の属性を GetSum し、色だけを変えた差分を適用すると、線幅がそれぞれ保たれる）
  - `Geometry` の距離計算の代表的なケース

**完了条件（手動）**: 線幅の異なる 2 図形を選んで色を変えると、色だけが変わる。その後どちらか 1 つだけを選ぶと、ダイアログに色と線幅が正しく表示される。

---

### フェーズ 6: 入力・ズーム・ダイアログ

**対象ファイル**: `Zooming.h`、`View.h`、`FigureAttributeDialog.h/.cpp`、`MouseEventTranslator.h`、`Document.h`、`Geometry.h`

#### 6-1. [M3] ホイールズームの基準点
```cpp
window.ScreenToClient(&point);
point = DPtoLP(point);
```
- 同時に、`ShiftTo` 内の未使用変数を削除し、`OnDragEnd` の後で `isDragging = false` に戻します。

#### 6-2. [M5] ダイアログのコマンド処理
- `OnCommand` で、自前の処理の後に `return CDialogEx::OnCommand(wParam, lParam);` を呼びます。
- `OnOK()` をオーバーライドし、`UpdateData(TRUE)` だけを行います。Enter キーで値が適用され、ダイアログは閉じません。
- `OnCancel()` は現状どおり、ダイアログを隠すだけです。

#### 6-3. [L1] 右クリックをクリックとして扱わない
- `MouseEventTranslator::OnRButtonUp` で、ドラッグしていなければ `OnClick` を送らずに `Clear()` だけを行います。
- 右ドラッグによるパン（`OnDragEnd(MK_RBUTTON, ...)`）は現状どおり残します。

#### 6-4. [L2] 描画領域の外でドラッグを終えたとき
- `Geometry` に `static CPoint Clamp(CPoint point, const CRect& area)` を追加します。
- `Document::OnDragEnd` では、`IsValid` で捨てずに、`Clamp` した点を `commandManager.OnDragEnd` に渡します。
- `OnDragging` も同様に `Clamp` して渡し、範囲選択の枠が領域の端で止まるようにします。

#### 6-5. [L7] TRACE の引数を直す
- `MouseEventTranslator.h:46` の余分な `methodName` を削除します。

**完了条件（手動）**:
- Ctrl+ホイールで、マウスカーソルの位置を中心に拡大縮小する。
- ダイアログが × ボタンと Esc で隠れ、Enter では値が適用されて閉じない。
- 選択モードで右クリックしても、選択状態が変わらない。
- 領域外まで範囲選択の枠をドラッグして離すと、領域内の図形が選択され、枠が残らない。

---

### フェーズ 7: 保守性の改善（任意）

機能を変えずに整理する変更です。他のフェーズとは**別のコミット**にし、差分を読みやすくします。

| 項目 | 内容 |
|---|---|
| 使われていないコード | コメントアウトされた `OnDragging` を削除する。`CacheDevice` は公開 API として残し、旧名の別名 `CashDevice` だけを削除する（D4） |
| 綴り | `Hilight` → `Highlight`、`DrawSelecter` → `DrawSelector`、`erace` → `erase`、`maxmum` → `maximum`、`searchinRect` → `searchingRect`（機械的に一括置換。`Cash` → `Cache` は対応済み） |
| [D1] 名前空間の漏れ | `View.h` のグローバル スコープの `using namespace Shos::MFCDoubleBuffer;` をやめる。`View.h` では修飾名（または `View` クラス内の `using` 型別名）を使い、`View.cpp` では `using namespace` してよい |
| [D2] 作業用ビットマップの再利用 | `DoubleBuffer` に作業用ビットマップをメンバーとして持たせ、クライアント領域の大きさが変わったとき（`Update()` / `OnSize`）だけ作り直す |
| [D3] 描画失敗時の代替 | キャッシュや作業用ビットマップを作れなかったときは、背景色で塗りつぶして Layer 1 と Layer 2 を画面の DC に直接描く |
| [D4] `CacheDevice` の前提条件 | キャッシュがまだない状態で使われたことを `ASSERT` で検出し、その前提をコメントに明記する |
| [D5] コメントのファイル名 | `DoubleBuffer.h` / `DoubleBuffer.cpp` の冒頭コメントの `MFCDoubleBuffer.*` を実際のファイル名に直す（またはファイル名を `MFCDoubleBuffer.*` に変えてプロジェクトを更新する） |
| [D6] 名前空間の命名 | `shos` と `Shos::MFCDoubleBuffer` のどちらの書き方に揃えるかを決めて統一する。アプリ本体のクラスを名前空間に入れるかどうかもあわせて決める |
| `override` | 仮想関数をオーバーライドしている箇所すべてに `override` を付ける |
| `DotFigure::radius` | `static constexpr long radius = 10L;` にする |
| `Zooming` | メンバ初期化子の順序を宣言順に揃える（C5038） |
| 乱数 | `std::uniform_int_distribution` を使う。`GetRandomFigure` の `default` 分岐で `nullptr` を返し、呼び出し側で除外する |
| `Geometry::InRect` | 右端と下端を含めて判定する（`<=` で比較）。`GetArea` の 1 つ目の矩形も正規化し、ループ変数を `size_t` にする |
| `AddDummyData` | トランザクションで囲み、1 回の Undo でまとめて取り消せるようにする |
| README | 機能一覧のインデントをスペースに統一する。ビルド方法とテストの実行方法を追記する |

---

## 4. 変更対象ファイルとフェーズの対応

| ファイル | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| `undo_redo_vector.h` | ● | | | | | | |
| `Observer.h` | | ● | | | | | |
| `MouseEventTranslator.h` | | ● | | | | ● | |
| `View.h` / `View.cpp` | | ● | | ● | | | ● |
| `Model.h` | | | ● | ● | ● | | ● |
| `Document.h` | | | ● | ● | | ● | |
| `Application.h` | | | ● | | | | |
| `ClipboardHelper.h` / `.cpp` | | | | ● | | | |
| `FigureAttribute.h` | | | | | ● | | |
| `FigureAttributeDialog.h` / `.cpp` | | | | | ● | ● | |
| `Zooming.h` | | | | | | ● | ● |
| `Geometry.h` | | | | | | ● | ● |
| `Figure.h` | | | | | | | ● |
| `Command.h` | | | | | | | ● |
| `DoubleBuffer.h` / `.cpp` | | | | | | | ● |
| テスト関連 | ● | | | | ● | | |
| `README.md` | | | | | | | ● |

---

## 5. 確認方法

各フェーズの終わりに、Windows 上で次の確認を行います。

1. `Debug` / `Release` × `x64` / `Win32` の全構成でビルドし、警告がフェーズ 0 より増えていないこと。
2. `vstest.console.exe x64\Debug\Shos.MiniCadSample.Test.dll` がすべて成功すること。
3. `MemoryLeakTest` を Debug で実行し、リーク報告がないこと。
4. アプリを Debug で起動し、各フェーズの「完了条件（手動）」を確かめること。終了時に、出力ウィンドウにメモリリーク報告（`Detected memory leaks!`）が出ないこと。
5. フェーズ 0 で保存したファイルを、修正後のアプリで開けること（ファイル形式の互換性）。

最後に、全フェーズを通した回帰確認として、レビューの各指摘の再現手順を一通り実行します。

---

## 6. リスクと対策

| リスク | 対策 |
|---|---|
| クリップボードの形式を変えると、旧バージョンとの間でコピー＆貼り付けできなくなる | サンプルアプリなので許容する。README に記載する |
| テストプロジェクトで MFC を使うと、ビルド時間が延び、構成が複雑になる | 影響が大きければ案 B（MFC に依存しない部分の切り出し）に切り替える |
| `undo_redo_vector.h` の改訂で、既存の Undo の粒度や動作が変わる（入れ子トランザクション、トランザクション中の `undo` 禁止、イテレータの `const` 化） | 1-1 で既存テスト（`undo_group`、`clear` など）とアプリの手動確認を通し、改訂版だけを先に単独でコミットする |
| 改訂版で読み込むヘッダー（`<shared_mutex>` など）が増え、MFC アプリのビルド時間が延びる | 影響が大きければ、スレッドセーフ版を別のヘッダーに分ける（N4） |
| フェーズ 7 の一括置換による取りこぼし | 置換後に全構成でビルドし、`grep` で旧名称が残っていないことを確かめる |

---

## 7. 作業量の目安

| フェーズ | 目安 |
|---|---|
| 0 | 0.5 日 |
| 1 | 1 日（M7/M8 は解消済みだが、新機能のテストが増えるため据え置き） |
| 2 | 0.5 日 |
| 3 | 1 日 |
| 4 | 1.5 日 |
| 5 | 1 日 |
| 6 | 0.5 日 |
| 7 | 0.5 日 |
| 合計 | 約 6.5 日 |

---

## 8. 今回の対象外（今後の検討事項）

- **図形の ID 化**: ポインタではなく ID で図形を識別し、ハイライト、`Hint`、選択での寿命の問題を根本から防ぐ。
- **選択状態を Model の外に出す**: 選択を View の状態として持ち、Undo で選択状態が巻き戻らないようにする。
- **描画キャッシュの差分更新**: Layer1 を全部作り直すのをやめ、無効化した範囲だけを描き直す。今回の更新で、キャッシュに差分だけを描き足せる `DoubleBuffer::CacheDevice` が使えるようになりました。たとえば `Hint::Type::Added` のときは `Update()` を呼ばず、追加された図形を `CacheDevice` に描き足してから `InvalidateRect(..., FALSE)` する、という形で `View::OnUpdate` に組み込めます。
- **シリアライズのバージョン管理**: `VERSIONABLE_SCHEMA` を導入し、将来の形式変更に備える。

---

## 9. 進捗

### フェーズ 0（2026-11-08 完了）

- ベース: `main` の `fe480e6` から `fix/review-2026-11-08` を作成した。前提となる変更（`undo_redo_vector.h` の改訂、ダブルバッファ関連の更新）はコミット済みで、ブランチに含まれている。計画書の更新は `4c0a421` としてコミットした。
- ベースライン（WSL から Visual Studio 18 の MSBuild と vstest を実行）:

  | 項目 | 結果 |
  |---|---|
  | ビルド（`Debug` / `Release` × `x64` / `x86`） | すべて成功。警告は 2 種類（`DoubleBuffer.h(101)` C4458、`Geometry.h(106)` C4018） |
  | 単体テスト（x64 / x86 Debug） | 14 件すべて成功 |
  | `MemoryLeakTest` | 終了コード 0。ただしリークの報告先がデバッガーの出力だけだったため、コマンド ラインからは判定できなかった（1-6 で対応） |

- 6. の実機での手動確認: リポジトリ所有者が実施し、**3 件とも再現した**。いずれも計画どおり対応する。
  - **H2**: 「新規作成」を 2 回した後、Dot で 1 回クリックすると点が 2 つ増える（フェーズ 2 で対応）
  - **M2**: 貼り付けた図形がすぐに表示されない（フェーズ 4 で対応）
  - **M5**: 属性ダイアログを × ボタンや Esc で閉じられない（フェーズ 6 で対応）

### フェーズ 1（2026-11-08 完了）

| 項目 | 状態 | 内容 |
|---|---|---|
| 1-0 / 1-1 | 完了 | 全構成でビルドが成功し、警告の種類もベースラインから増えていない。アプリの手動確認（ちらつき、リサイズ、ズーム、パン、`SCROLL_VIEW` 有効時のスクロール、Undo/Redo、削除、属性変更）は、リポジトリ所有者が実施して完了した。 |
| 1-2 | 完了 | `clear_history()` を `undo_redo_collection` と `synchronized_undo_redo_collection` に追加した。 |
| 1-3 [N1] | 完了 | 確保を `detail::reserve_at_least` で倍々に増やすようにした（`<windows.h>` の `max` マクロを避けるため `(std::max)` と書いた）。5 万回の `push_back` と、5 万要素を `clear()` して Undo する処理が、修正前の 151 秒から修正後は約 0.2 秒になった。旧実装に一時的に戻すと、テストが失敗することも確かめた。 |
| 1-4 [N2][N3] | 完了 | 下記「計画からの変更点」を参照。 |
| 1-5 [T1][T3] | 完了 | テストを 14 件から 36 件にした。全構成（x64 / x86 × Debug / Release）ですべて成功。 |
| 1-6 [T2] | 完了 | `undo_redo_pointer_vector_memory_leak_test` などを呼ぶようにした。リークを stderr に出力し、終了コードで判定できるようにした（リークなし 0 / あり 1）。わざとリークさせる `--self-check` で、検出できることを確かめた。x64 / x86 の Debug でリークなし。 |

**計画からの変更点**:
- **N2（例外時の所有権）**: 計画では `redo` を `try` で囲むだけだったが、`redo` の前のメモリ確保（ステップの `make_unique`、履歴の `reserve`）が失敗した場合にも要素がリークしていた。そこで次のようにした。
  - ステップを確保した後で要素をムーブするようにした。
  - 確保の失敗も含めて、渡された要素を `clean_up` するようにした。
  - 失敗の途中で所有者が曖昧にならないよう、要素の型にムーブ・ムーブ代入・`swap` が例外を投げないことを `static_assert` で求めるようにした（`int`、ポインター、`std::unique_ptr` などは満たす）。
  - 確保の N 回目を失敗させるテスト（`operator new` の差し替え）で、リークも二重解放もなく、コレクションと履歴が変わらないことを確かめた。
- **Undo/Redo 中の失敗**: `std::vector` は容量を減らさないので、Undo/Redo では確保が起きないことが分かった。これをテスト（`vector_undo_redo_does_not_allocate`）で確かめ、コメントに明記した。`std::list` では、トランザクションの Undo/Redo が途中で失敗すると中途半端な状態になる可能性があり、これを制限事項としてコメントに記載した（1 ステップなら状態は変わらないことをテストで確認）。
- **ムーブ後の `clean_up`（追加の修正）**: ムーブ元の `clean_up` が `nullptr` になっていたため、ムーブ元の `undo_redo_pointer_vector` を再利用すると黙ってリークしていた。ムーブ元にも `clean_up` を残すように変えた（データは空にするので、二重解放にはならない）。
- **N4（スレッドセーフ版）**: 残す前提でテストを追加した（基本操作、`modify`、8 スレッドからの同時 `push_back`）。

### フェーズ 2（2026-11-08 完了）

| 項目 | 状態 | 内容 |
|---|---|---|
| 2-1 [L5] | 完了 | `Observable::RemoveObserver` で `std::remove` の後に `erase` するようにした。登録されていなかった場合は `false` を返す。 |
| 2-2 [H2] | 完了 | `Observable::AddObserver` と `MouseEventTranslator::AddListener` を冪等にした（登録済みなら `false` を返して何もしない）。`View::OnInitialUpdate` では、リスナー登録とオブザーバー登録を初回だけ行い、`Application::Set` による属性の表示は毎回行うようにした。 |

- 単体テストに `observable_test` の 3 件を追加した（登録済みの相手を再登録しても通知は 1 回、削除が実際に効く、削除後に再登録できる）。合計 39 件。
- 全構成（`Debug` / `Release` × `x64` / `x86`）でビルドが成功し、警告はベースラインから変わっていない。単体テストはすべて成功し、`MemoryLeakTest`（x64 / x86 Debug）でリークなし。
- 完了条件の手動確認（新規作成を 3 回した後、Dot で 1 回クリックすると点が 1 つだけ増える。ダイアログで線幅を 1 回変えた後、Ctrl+Z を 1 回押すと元に戻る）: リポジトリ所有者が実施し、完了した。

### フェーズ 3（2026-11-08 完了）

| 項目 | 状態 | 内容 |
|---|---|---|
| 3-1 [H4] | 完了 | `Model::InvalidateHighlight()` を追加した。`Add`、`AddDummyData`、`RemoveSelectedFigures`、属性の `Update`、`Undo`、`Redo`、ファイル読み込み、`Append`（クリップボード）、`Reset`（`Clear`）で、ハイライトを解除する。 |
| 3-2 [M6] | 完了 | 使われていない `Model::Remove`、`Model::Change`、`Document::Add` を削除した。 |
| 3-3 [L3] | 完了 | `RemoveSelectedFigures` で、選択が空なら何もしない（Undo ステップも通知もなく、変更フラグも立たない）。 |
| 3-4 [L4] | 完了 | 属性変更の通知 `Hint(Changed, ...)` に、旧図形と新図形の両方を含めた。あわせて `Update(Figure&, Figure&)` で、旧図形が見つからなかったときに複製がリークしていた問題を直した（`bool` を返し、複製を削除する）。 |
| 3-5 [M1] | 完了 | `Model::Serialize`（ファイル専用）の読み込み後に `clear_history()` を呼び、`Hint::Type::All` を通知するようにした。 |
| 3-6 [L6] | 完了 | `Reset()` を、通知しない `Clear()`（private）と、通知する `Reset()` に分けた。`~Model` は `Clear()` だけを呼ぶ。`Application::Set` と `SetFigureAttributeObserver` は、メイン ウィンドウがない（または破棄済み）なら何もしないようにした（`GetMainFrame()`）。 |

**計画からの変更点（3-5 の前倒し）**: これまで、クリップボードからの貼り付けも `Document::Serialize` → `Model::Serialize` を通っていた。そのため、3-5 だけを行うと、貼り付けのたびに Undo 履歴が消える後退が起きる。これを避けるため、フェーズ 4 の一部を前倒しして、貼り付けを `Model::Append(CArchive&)`（従来どおり、図形ごとに Undo できる形で追加）に分けた。`ClipboardHelper::OnEditPaste` は、読み込み処理をコールバック（`std::function<void(CArchive&)>`）で受け取るようにした（4-5 のインターフェースに合わせた）。貼り付けた図形がすぐに表示されない問題（M2）と、Undo が 1 図形ずつになる問題は、フェーズ 4 で対応する。

**確認結果**:
- `Debug|x86`、`Release|x64`、`Release|x86` でビルドが成功し、新しい警告は出ていない。単体テストは 39 件すべて成功し、`MemoryLeakTest`（x86 Debug）でリークなし。
- `Debug|x64` は、最初は Visual Studio がソリューションを開いていたため、中間ファイル（`vc145.pdb` など）がロックされてビルドできなかった。Visual Studio を閉じた後に再ビルドして成功し、新しい警告はなかった。単体テスト 39 件はすべて成功し、`MemoryLeakTest`（x64 Debug）でリークなし。
- 完了条件の手動確認（開いた直後は Undo が無効、H4 の再現手順でクラッシュせず枠も残らない、選択なしの Delete で変更マークが付かない）: リポジトリ所有者が実施し、すべて問題なかった。

### フェーズ 4（2026-11-08 完了）

| 項目 | 状態 | 内容 |
|---|---|---|
| 4-1 | 完了 | 仕様どおり。Copy は選択があれば選択図形だけ、なければ全図形。Cut は選択がないと無効。Paste は 1 回の Undo で取り消せ、貼り付けた図形だけが選択される。 |
| 4-2 | 完了 | `Model` に `StoreFigures`、`GetFiguresArea`、`DrawFigures`（選択マーカーなしで描く。`Figure::Draw(dc, drawSelector)` を追加）、`Paste`、`HasSelection`、`IsEmpty` を追加した。`Paste` は全部か無しにした（下記）。 |
| 4-3 [H1] | 完了 | Cut は「Copy（選択図形）＋ `RemoveSelectedFigures`」にした。`DeleteContents` は呼ばない。 |
| 4-4 [M4] | 完了 | 独自形式を `RegisterClipboardFormat(_T("Shos.MiniCadSample.Figures"))` で登録した。`globalMemoryHandle`、`OnDestroyClipboard`、`ON_WM_DESTROYCLIPBOARD` を削除した。読み込みの失敗（`CException*`、`std::exception`）は `Paste` が `false` を返し、View は警告音を鳴らす。 |
| 4-5 | 完了 | `ClipboardHelper::Copy(window, area, background, store, draw)`、`Paste(window, load)`、`CanPaste()`、`GetFormat()` にした。`area` の左上を原点に移す座標変換とクリッピングは `ClipboardHelper` が行う。選択がある場合、`area` は選択図形の外接矩形（線幅と余白を含む）。全図形の場合はドキュメント全体で、従来どおり描画領域の背景色も描く。クリップボードは RAII（`ClipboardCloser`）で必ず閉じる。`SetClipboardData` に失敗したときはハンドルを解放する。 |
| 4-6 | 完了 | Cut、Copy、Paste に `ON_UPDATE_COMMAND_UI` を追加した。Copy は図形が 1 つ以上あるとき、Cut は選択があるとき、Paste は独自形式のデータがあるときに有効。 |

**Paste の全部か無しの実装**:
1. `Model::ReadFigures` で一時的な `std::unique_ptr` のリストに読み込む。次の場合は `CArchiveException` を投げ、読み込み済みの図形は解放される。
   - 件数どおりのデータがない
   - `Figure` 以外のクラスが含まれる（`badClass`）
   - 同じオブジェクトへの参照が重複している（`badIndex`。二重所有を防ぐ）
2. 確定の前に、メモリを確保する処理をすべて済ませる（通知用の `Hint`、ダイアログに表示する属性）。
3. 新しく追加した `undo_redo_collection::append` で、1 つの Undo ステップとして追加する。`append` は全部か無しで、失敗すると要素をすべて `clean_up` し、コレクションと履歴は変えない。`std::list` のようにノードの確保で途中失敗する場合は、適用済みのものを Undo して巻き戻す。
4. 確定した後は、選択の変更と通知だけを行う。この部分で Model がメモリを確保することはない。

ファイルの読み込み（`Model::Serialize`）も、同じ読み込みと確定の処理を使うようにした。そのため、壊れたファイルを開いても図形が途中まで追加されることはなくなった。

**テスト（計画からの変更点：5-3 の案 A を前倒し）**: Paste の失敗テストには `Model`（MFC）が必要なため、テスト プロジェクトの `UseOfMfc` を `Dynamic` にした。`pch.h` ではアプリの `framework.h` を読み込み、アプリの `Figure.cpp` と `Geometry.cpp` もコンパイルするようにした。
- `undo_redo_vector`: `append` の 4 件（基本、トランザクション内、確保失敗の注入、`std::list` での巻き戻し）。
- `Model`（`ModelTest.cpp`）: 7 件。
  - 選択図形の Copy と Paste（原点から離れた位置の外接矩形、貼り付け後の選択状態、1 回の Undo）
  - 選択なしでの全図形の Copy
  - 空のデータ
  - 件数不足のデータ
  - `Figure` 以外のクラス
  - 重複参照
  - 確保失敗の注入
  - 選択なしの削除（L3）

  失敗した場合は、図形、選択、Undo と Redo の状態が変わらないこと、`_CrtMemCheckpoint` でリークがないこと（Debug）を確かめる。一時的にリークを入れると、テストが失敗することも確かめた。
- 合計 51 件。全構成（`Debug` / `Release` × `x64` / `x86`）でビルドが成功し、警告はベースラインから増えていない。テストはすべて成功し、`MemoryLeakTest`（x64 / x86 Debug）でリークなし。
- テスト環境の注意: Debug ビルドの MSVC STL は、`noexcept` のコンストラクタ（既定、ムーブ）でもコンテナ proxy を確保する。これを失敗させると `std::terminate` になり、テストが止まる。そのため、確保失敗の注入では proxy の確保を対象外にした（`Shos.MiniCadSample.Test.cpp` の `operator new`）。

- 完了条件と 4-5 の受け入れ確認（実機）: リポジトリ所有者が実施し、すべて問題なかった。確認した項目は次のとおり。
  - Cut で選択図形だけが消え、Undo で戻る
  - Paste がすぐに表示され、1 回の Undo で取り消せる
  - 2 つのインスタンスの間で Copy と Paste ができる
  - Word に EMF として貼り付けられる
  - 原点から離れた選択図形を、独自形式、EMF、DIB のいずれでも余白なく出力できる
  - Cut、Copy、Paste の有効と無効が正しく切り替わる

### フェーズ 5（2026-11-08 完了）

| 項目 | 状態 | 内容 |
|---|---|---|
| 5-1 [H3] | 完了 | `FigureAttribute` に `Empty()`、`ApplyTo(target)`（有効な項目だけを上書きし、変わったかどうかを返す）、`IsEmpty()` を追加した。`SetColor` と `SetPenWidth` は項目を有効にし、値か有効フラグが変わったときに `true` を返す。ダイアログは、変更した項目だけが有効な差分を通知し、表示用の `figureAttribute` にも同じ差分を適用する。`Model::Update(change)` は、選択なしなら新規図形用の属性に、選択ありなら各複製に差分を適用する。 |
| 5-2 | 完了（計画から変更） | 下記のとおり、1 回の操作で変わる項目は常に 1 つになったため、まとめる処理は不要になった。 |
| 5-3 [T3] | 完了 | MFC 化はフェーズ 4 で済んでいる。テストを 19 件追加した（合計 70 件）。 |

**計画からの変更点（5-2）**: これまでは、色ボタンと「Set」ボタンのどちらを押しても `UpdateData(TRUE)` で色と線幅の両方を保存していた。そのため、色が混在している（ダイアログでは自動色で表示される）ときに線幅を変えると、表示中の自動色まで全図形に適用されるおそれがあった。これは H3 と同じ種類の問題である。そこで、`OnCommand` で押されたボタンに対応する項目だけを読むようにした。色ボタンでは色だけ、「Set」ボタンでは線幅だけを読む。`DoDataExchange` は表示（読み込み）だけを行う。線幅の欄が空（線幅が混在）のときは何もしない。数値にならない値（大きすぎる値）では警告音を鳴らす。

**その他の変更**:
- 差分を適用しても属性が変わらない図形は、置き換えない（Undo ステップを作らない）。1 つも変わらない場合は通知もしない。
- 複製は `std::unique_ptr` で保持し、適用しない場合に解放する。

**テスト**:
- `figure_attribute_test`（9 件）: 既定値、`Empty`、`Set` で有効になること、`GetSum`（空、一致、混在）、`ApplyTo`（有効な項目だけを上書き、無効な項目を有効にする）、H3 の再現（線幅 1 と 3 の属性に色だけの差分を適用すると、線幅が保たれる）。
- `geometry_test`（7 件）: 2 点間の距離、線分までの距離（垂線、線分上、両端の外、長さ 0）、矩形までの距離、円と楕円までの距離、つぶれた楕円、`GetArea`（正規化していない矩形を含む）。
  - `GetDistanceToEllipse` は近似である。円の上と長軸方向は正確だが、横長の楕円の短軸方向では距離を大きく見積もる（例: 幅 200、高さ 100 の楕円の中心から下へ 100 の点は、実際は 50 だが 100 になる）。今回は動作を変えず、正確な場合だけをテストした。
- `model_test`（3 件）: 線幅の異なる 2 図形の色だけを変えると線幅が保たれ、1 回の Undo で戻ること。効果のない変更では Undo ステップも通知も作られないこと。選択なしでは新規図形用の属性の該当項目だけが変わること。
- 全構成（`Debug` / `Release` × `x64` / `x86`）でビルドが成功し、テスト 70 件がすべて成功した。`MemoryLeakTest`（x64 / x86 Debug）でリークなし。テスト プロジェクトが `Geometry.h` を読み込むようになったため、既存の `Geometry.h(106)` の C4018 が x86 ではテスト プロジェクトでも出る（警告の種類は増えていない）。

- 完了条件の手動確認（実機）: リポジトリ所有者が実施し、すべて問題なかった。確認した項目は次のとおり。
  - 線幅の異なる 2 図形の色だけを変えると、色だけが変わる
  - その後 1 つだけを選ぶと、ダイアログに色と線幅が正しく表示される
  - 色の異なる図形の線幅を「Set」で変えても、色は変わらない
  - 1 回の Undo で色の変更が戻る

### フェーズ 6（2026-11-08 完了）

| 項目 | 状態 | 内容 |
|---|---|---|
| 6-1 [M3] | 完了 | `Zooming::OnMouseWheel` で、スクリーン座標を `ScreenToClient` でクライアント座標にし、`DPtoLP` の戻り値を使うようにした。`ShiftTo` 内の未使用変数を削除し、`OnDragEnd` の後で `isDragging` を `false` に戻すようにした。 |
| 6-2 [M5] | 完了 | `OnCommand` の戻り値を `BOOL` にし、色ボタンと「Set」ボタン以外は `CDialogEx::OnCommand` に渡すようにした。× ボタンと Esc は `OnCancel` でダイアログを隠す。`OnOK`（Enter）は線幅を適用し、ダイアログは閉じない。フェーズ 5 で `DoDataExchange` を表示専用にしたため、`OnOK` は `UpdateData(TRUE)` ではなく `SavePenWidth()` を呼ぶ。 |
| 6-3 [L1] | 完了 | `MouseEventTranslator::OnRButtonUp` で、ドラッグしていなければ `OnClick` を送らないようにした。右ドラッグによるパンは従来どおり。 |
| 6-4 [L2] | 完了 | `Geometry::Clamp`（`PtInRect` と同じく、右端と下端を含まない）を追加した。`Document` は、領域内で始まったドラッグだけをコマンドに渡す。その後のドラッグ中と終了時の点は領域内に `Clamp` して渡す（下記）。 |
| 6-5 [L7] | 完了 | `TRACE` の余分な引数 `methodName` を削除した。 |

**計画からの変更点（6-4）**: 計画どおり `OnDragging` と `OnDragEnd` で `Clamp` するだけだと、領域外で始まったドラッグが領域内に入ったときに、`OnDragStart` を受けていないコマンドへ `OnDragging` と `OnDragEnd` が届く。`SelectCommand` は前回の始点のまま範囲選択をしてしまう。そこで `Document` に `isDragging` を持たせ、ドラッグが領域内で始まった場合だけ、そのドラッグの残りをコマンドに渡すようにした。これまでは、ドラッグ中に領域外へ出ると `OnDraggingAbort` していたが、今後は端で止まったまま続く。

**テスト**: `geometry_test` に `Clamp` のテストを 1 件追加した（合計 71 件）。その他の変更は、ウィンドウやマウス入力に依存するため、手動で確認する。

**ビルドとテスト**:
- `Debug x86`、`Release x86`、`Release x64` は、リポジトリ内でビルドが成功し、テスト 71 件がすべて成功した。警告はベースラインから増えていない。`MemoryLeakTest`（x64 / x86 Debug）でリークなし。
- `Debug x64` は、リポジトリ内の中間ファイル `Shos.MiniCadSample\x64\Debug\vc145.pdb` への書き込みが C1090 や C1041 で失敗し続けた（Dropbox の同期によるロックと思われる）。そこで、出力先を一時フォルダーにしてビルドし、成功を確認した。テスト 71 件もすべて成功し、`MemoryLeakTest` でもリークはなかった。

- 完了条件の手動確認（実機）: リポジトリ所有者が実施し、すべて問題なかった。確認した項目は次のとおり。
  - Ctrl+ホイールで、マウスカーソルの位置を中心に拡大縮小する
  - ダイアログが × ボタンと Esc で隠れ、メニューから再表示できる。Enter では線幅が適用され、閉じない
  - 選択モードで右クリックしても選択状態が変わらない。右ドラッグによるパンは使える
  - 領域外まで範囲選択の枠をドラッグして離すと、領域内の図形が選択され、枠が残らない
  - 線や矩形を領域外までドラッグして描くと、終点が端で止まり、次のクリックで新しい図形を描き始める

### フェーズ 7（2026-11-08 実施。手動確認は未実施）

| 項目 | 状態 | 内容 |
|---|---|---|
| 使われていないコード（D4） | 完了 | `View` と `Zooming` のコメントアウトされた `OnDragging`、旧名の別名 `CashDevice` を削除した。 |
| 綴り | 完了 | `Hilight` → `Highlight`、`DrawSelecter` → `DrawSelector`、`maxmum` → `maximum`、`searchinRect` → `searchingRect`。`erace` はすでになかった。 |
| D1 | 完了 | `View.h` のグローバルな `using namespace` をやめた。基底クラスは `View` 内の別名 `Base` で参照する。`View.cpp` では `using namespace` する（`IMPLEMENT_DYNCREATE` などのマクロが修飾名を扱えないため）。 |
| D2 | 完了 | 作業用ビットマップをメンバーにした。キャッシュと同じくクライアント領域の大きさで、大きさが変わったときだけ作り直す（キャッシュも同様にした）。Layer 2 はクリップ ボックスでクリッピングする。 |
| D3 | 完了 | ビットマップを作れなかったときは、背景色で塗りつぶし、Layer 1 と Layer 2 を画面の DC に直接描く。 |
| D4 | 完了 | `CacheDevice` の前提条件（最初の WM_PAINT の後でしか使えない）をコメントに書き、`ASSERT` で検出する。 |
| D5 | 完了 | コメントのファイル名を `DoubleBuffer.h` / `DoubleBuffer.cpp` に直した。 |
| D6 | 完了（方針はリポジトリ所有者が決定） | アプリ本体のクラスを、役割ごとに名前空間に分けた（下記）。 |
| `override` | 完了 | `Figure`、`Command`、`Document`、`View`、`Application`、`MouseEventTranslator::TestListener` で、オーバーライドしている仮想関数すべてに付けた。 |
| `DotFigure::radius` | 完了 | `static constexpr long radius = 10L;` にした。 |
| `Zooming` | 完了 | メンバ初期化子の順序を宣言順に揃えた。 |
| 乱数 | 完了 | `std::uniform_int_distribution` を使う。`GetRandomFigure` の `default` 分岐は `nullptr` を返し、`GetRandomFigures` で除外する。 |
| `Geometry::InRect` | 完了 | 右端と下端を含めて判定する。両方の矩形を正規化する。`GetArea` は 1 つ目の矩形も正規化し、ループ変数を `size_t` にした。 |
| `AddDummyData` | 完了 | トランザクションではなく `append` で、1 回の Undo で取り消せるようにした（全部か無し）。通知用の `Hint` は確定の前に作る。 |
| README | 完了 | 機能一覧のインデントをスペースに統一した。開発環境（C++20、Visual Studio 2026）、ビルド方法、テストの実行方法を追記した。 |

**D6 の名前空間の構成**: リポジトリ所有者の指示（「Models、Views、Controllers など、適切な namespace に分けたい」）に従った。

| 名前空間 | クラス |
|---|---|
| `Shos::MiniCadSample` | `Application` |
| `Shos::MiniCadSample::Models` | `FigureAttribute`、`Figure` とその派生クラス、`FigureHelper`、`Hint`、`Model` |
| `Shos::MiniCadSample::Views` | `View`、`MainFrame`、`FigureAttributeDialog`、`Zooming`、`ClipboardHelper` |
| `Shos::MiniCadSample::Controllers` | `Document`、`MouseEventTranslator`、`Cursor`、`Command` とその派生クラス、`CommandManager` |
| `Shos::MiniCadSample::Utilities` | `Geometry`、`Observer`、`Observable` |
| `Shos` | `GdiObjectSelector`（アプリとダブルバッファ ライブラリの両方が使う） |
| `Shos::MFCDoubleBuffer` | 変更なし（ライブラリ） |
| `shos` | 変更なし（`undo_redo_vector.h`。STL 風の命名のライブラリ） |

- `Document` は `Controllers` に置いた。MFC では `CDocument` はモデル側だが、このアプリの `Document` は `Model` と `CommandManager` を持ち、マウス イベントをコマンドに振り分ける仲介役である。また `Models` に置くと、`Models` が `Controllers`（`CommandManager`）に依存してしまう。
- 依存の向きは Views → Controllers → Models → Utilities とした。`using namespace` は名前空間ブロックの中だけで使い、グローバル スコープでは使わない。
- MFC のマクロ（`IMPLEMENT_SERIAL` など）は修飾されていないクラス名を受け取るため、`.cpp` では名前空間ブロックの中に書いた。
- ファイルとクリップボードのデータに保存されるクラス名は、修飾されていない名前（`"RectangleFigure"` など）のままである。そのため、ファイル形式は変わらず、以前のバージョンで保存したファイルも読み込める。これを確かめるテストを 1 件追加した。
- 依存していたインクルードの順序を明示した。`MouseEventTranslator.h` は `Geometry.h` を、`View.h` は `Document.h` を読み込む。

**計画外の修正**:
- 既存の警告 2 件を解消した。
  - `DoubleBuffer.h` の C4458: `SetBackgroundColor` の引数名がメンバー名を隠していた。
  - `Geometry.h` の C4018: `GetArea` のループ変数を `size_t` にした。
- `SCROLL_VIEW` を有効にする（`ZOOMING_VIEW` を無効にする）とビルドできなかった。以前からの問題で、右ドラッグによるパンのハンドラーが `#ifdef ZOOMING_VIEW` の外で `zooming` を使っていた。ハンドラーを `#ifdef` で囲み、`SCROLL_VIEW` でのビルド（Debug x86）を確かめた。

**テスト**: 3 件追加した（合計 74 件）。
- `AddDummyData` が 1 回の Undo で取り消せること
- `InRect` の境界（辺に接する場合、正規化していない矩形）
- 図形のクラス名が修飾されていないこと（ファイル形式の互換性）

**ビルドとテスト**: 全構成（`Debug` / `Release` × `x86` / `x64`）で、リポジトリ内でビルドが成功した。フェーズ 6 で失敗していた `Debug x64` も成功した。**警告は 0 件**になった。テスト 74 件がすべて成功し、`MemoryLeakTest`（x64 / x86 Debug）でリークなし。

**未実施**:
- 手動確認（実機）。確認する項目は次のとおり。
  - 描画とちらつき（D2 と D3 で描画処理を変えたため）
  - 範囲選択の枠にぴったり接する図形が選択されること
  - 「Random」で追加した図形が 1 回の Undo で消えること
  - 以前のバージョンで保存したファイルを開けること（名前空間の導入後）
