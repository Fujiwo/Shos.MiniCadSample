# レビュー指摘の修正計画

- 作成日: 2026-11-08
- 元にしたレビュー: [`Reviews/2026-11-08-repository-review.md`](../Reviews/2026-11-08-repository-review.md)
- 対象コミット: `ae5c882`（main）
- 更新: 2026-11-08 — 作業ツリー上の `undo_redo_vector.h` の全面改訂を反映しました。M7 と M8 は解消済みなので、フェーズ 1 を縮小し、新しい指摘 N1〜N4 を加えました（レビュー 8 章）。

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
| 5 | 図形属性の受け渡し | H3, T3（一部） | 0（3 の後が望ましい） |
| 6 | 入力・ズーム・ダイアログ | M3, M5, L1, L2, L7 | 0 |
| 7 | 保守性の改善（任意） | 5 章の項目 | 1〜6 |

フェーズ 2、5、6 は互いに独立しているので、並行して進めても構いません。

---

## 3. 各フェーズの詳細

### フェーズ 0: 準備

1. `main` から `fix/review-2026-11-08` を作成します。
2. Windows 上で `Debug|x64` と `Debug|Win32` をビルドし、警告の数を記録します。後で比べるためです。
3. `Shos.MiniCadSample.Test` を実行し、現状の結果を記録します。
4. `Shos.MiniCadSample.MemoryLeakTest` を Debug で実行し、出力ウィンドウにリーク報告があるか記録します。
5. レビューで「要実機確認」とした項目を、実際に動かして確かめます。
   - **M2**: 貼り付けた図形がすぐに表示されないか。
   - **M5**: × ボタンや Esc でダイアログが閉じないか。
   - **H2**: 新規作成を 2 回した後、Dot で 1 回クリックすると点が 2 つ増えるか。

   再現しなかった項目は、本計画から外すか、優先度を下げます。

---

### フェーズ 1: `undo_redo_vector` の改訂版の確定とテストの整備

**対象ファイル**: `Shos.MiniCadSample/undo_redo_vector.h`、`Shos.MiniCadSample.Test/Shos.MiniCadSample.Test.cpp`、`Shos.MiniCadSample.MemoryLeakTest/Shos.MiniCadSample.MemoryLeakTest.cpp`

> **現状**: `undo_redo_vector.h` は作業ツリー上で全面的に改訂されており（未コミット）、M7（`reset()` の null 参照）と M8（コピー禁止、入れ子トランザクション、`noexcept` のデストラクタ、`undo_data` の削除）は解消済みです。このフェーズでは、改訂版をビルドとテストで確かめてコミットし、残っている課題を片付けます。

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
- `execute` で `step->redo(data)` を `try` で囲みます。例外が出たら `clean_up` があれば `step->clean_up_elements(clean_up)` を呼び、例外を投げ直します。
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

> 「選択なしの Copy は全図形」としたのは、画像として図全体を貼り付ける現在の使い方を残すためです。選択がないときは Copy も無効にする案もあります。どちらにするかは、着手前にリポジトリ所有者が決めてください。

#### 4-2. Model への API 追加
- `void StoreFigures(CArchive& ar, bool selectedOnly) const`: 対象の図形を書き出します（件数 + `WriteObject`）。
- `void DrawFigures(CDC& dc, bool selectedOnly) const`: 画像出力用に描画します。
- `void Paste(CArchive& ar)`: 次の順に処理します。
  1. 図形を読み込む。
  2. 既存の図形の選択を解除する。
  3. トランザクションの中で `push_back` する。
  4. 貼り付けた図形を選択状態にする。
  5. `Hint(Added, 追加した図形)` を通知する。

  これで M2（表示されない、Undo が 1 図形ずつになる）を解決します。

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
| 使われていないコード | `DoubleBuffer::CashDevice` と、コメントアウトされた `OnDragging` を削除 |
| 綴り | `Hilight` → `Highlight`、`DrawSelecter` → `DrawSelector`、`Cash` → `Cache`、`erace` → `erase`、`maxmum` → `maximum`、`searchinRect` → `searchingRect`（機械的に一括置換） |
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
| `DoubleBuffer.h` | | | | | | | ● |
| テスト関連 | ● | | | | ● | | |
| `README.md` | | | | | | | ● |

---

## 5. 確認方法

各フェーズの終わりに、Windows 上で次の確認を行います。

1. `Debug|x64`、`Debug|Win32`、`Release|x64` でビルドし、警告がフェーズ 0 より増えていないこと。
2. `vstest.console.exe x64\Debug\Shos.MiniCadSample.Test.dll` がすべて成功すること。
3. `MemoryLeakTest` を Debug で実行し、リーク報告がないこと。
4. アプリを Debug で起動し、各フェーズの「完了条件（手動）」を確かめること。終了時に、出力ウィンドウにメモリリーク報告（`Detected memory leaks!`）が出ないこと。
5. フェーズ 0 で保存したファイルを、修正後のアプリで開けること（ファイル形式の互換性）。

最後に、全フェーズを通した回帰確認として、レビューの各指摘の再現手順を一通り実行します。

---

## 6. リスクと対策

| リスク | 対策 |
|---|---|
| フェーズ 4 で Copy の仕様（選択なしのとき）が変わる | 4-1 の仕様を着手前に確定する |
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
- **描画キャッシュの差分更新**: Layer1 を全部作り直すのをやめ、無効化した範囲だけを描き直す。
- **シリアライズのバージョン管理**: `VERSIONABLE_SCHEMA` を導入し、将来の形式変更に備える。
