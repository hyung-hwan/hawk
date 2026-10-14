"use strict";

(function () {
	const controls = {};
	const names = ["program", "input", "output", "title", "search", "examples", "run", "copy", "save", "save-copy", "delete", "notice", "new-example"];
	let examples = [];
	let tool = "sed";
	let selected_id = null;
	let busy = false;
	let output_text = "";
	const descriptions = {
		sed: ["SED COMMANDS", "INPUT TEXT", "Apply sed commands to your text.", "Commands use Hawk's sed syntax. The input's line endings are preserved."],
		m4: ["DEFINITIONS / PRELUDE", "M4 SOURCE", "Expand macros and generate text.", "The prelude is placed before the source. Use dnl to discard definition-line newlines."],
		mus: ["MUSTACHE TEMPLATE", "JSON DATA", "Turn structured data into text.", "{{name}} escapes HTML; {{{name}}} is raw. Sections iterate arrays and select content."]
	};

	for (const name of names) controls[name] = document.getElementById(name);

	function notice(message, kind) {
		controls.notice.textContent = message;
		controls.notice.className = "notice" + (kind ? " " + kind : "");
	}

	function update_counts() {
		document.getElementById("program-count").textContent = controls.program.value.length + " chars";
		document.getElementById("input-count").textContent = controls.input.value.length + " chars";
		document.getElementById("output-count").textContent = output_text.length + " chars";
	}

	function set_busy(value) {
		busy = value;
		for (const name of ["run", "save", "new-example", "program", "input", "title"]) controls[name].disabled = value;
		controls.delete.disabled = value || selected_id === null;
		controls["save-copy"].disabled = value || selected_id === null;
		for (const tab of document.querySelectorAll("[data-tool]")) tab.disabled = value;
		for (const button of controls.examples.querySelectorAll("button")) button.disabled = value;
		controls.run.textContent = value ? "Working…" : "Run · Ctrl ↵";
	}

	function set_tool(value) {
		tool = value;
		const description = descriptions[tool];
		document.getElementById("program-label").textContent = description[0];
		document.getElementById("input-label").textContent = description[1];
		document.getElementById("tool-description").textContent = description[2];
		document.getElementById("tool-hint").textContent = description[3];
		document.getElementById("tool-panel").setAttribute("aria-labelledby", "tab-" + tool);
		for (const tab of document.querySelectorAll("[data-tool]")) {
			tab.setAttribute("aria-selected", String(tab.dataset.tool === tool));
			tab.tabIndex = tab.dataset.tool === tool ? 0 : -1;
		}
	}

	function render_examples() {
		controls.examples.replaceChildren();
		const query = controls.search.value.toLowerCase();
		const visible = examples.filter(item => (item.title + " " + item.tool).toLowerCase().includes(query));
		document.getElementById("example-count").textContent = visible.length + " / " + examples.length;
		for (const item of visible) {
			const row = document.createElement("li");
			const button = document.createElement("button");
			const badge = document.createElement("span");
			const title = document.createElement("span");
			button.type = "button";
			button.disabled = busy;
			button.className = item.id === selected_id ? "active" : "";
			badge.className = "tool-badge";
			badge.textContent = item.tool;
			title.textContent = item.title;
			button.append(badge, title);
			button.addEventListener("click", () => { if (!busy) load_example(item); });
			row.append(button);
			controls.examples.append(row);
		}
		if (visible.length === 0) {
			const row = document.createElement("li");
			row.className = "empty";
			row.textContent = "No matching examples.";
			controls.examples.append(row);
		}
	}

	function clear_output() {
		output_text = "";
		controls.output.textContent = "Your result will appear here.";
		controls.copy.disabled = true;
		update_counts();
	}

	function load_example(item) {
		selected_id = item.id;
		set_tool(item.tool);
		controls.title.value = item.title;
		controls.program.value = item.program;
		controls.input.value = item.input;
		clear_output();
		set_busy(false);
		render_examples();
		notice("Loaded “" + item.title + "”. Edit it or run it.");
	}

	function new_example(value) {
		selected_id = null;
		set_tool(value);
		controls.title.value = "";
		controls.program.value = "";
		controls.input.value = value === "mus" ? "{}" : "";
		clear_output();
		set_busy(false);
		render_examples();
		notice("New " + value + " example.");
		controls.program.focus();
	}

	async function api(path, method, payload) {
		const options = { method: method || "GET", headers: { "X-Workbench-Request": "1" } };
		if (payload !== undefined) {
			options.headers["Content-Type"] = "application/json";
			options.body = JSON.stringify(payload);
		}
		const response = await fetch(path, options);
		const text = await response.text();
		let result;
		try { result = JSON.parse(text); }
		catch (_) { throw new Error(text.trim() || "The server returned an empty response."); }
		if (!response.ok || result.ok === false) throw new Error(result.error || "The request failed.");
		return result;
	}

	async function refresh_examples() {
		const result = await api("/api/examples");
		examples = result.examples;
		render_examples();
	}

	function payload() {
		return { tool: tool, title: controls.title.value, program: controls.program.value, input: controls.input.value };
	}

	async function run() {
		if (busy) return;
		try {
			if (tool === "mus") JSON.parse(controls.input.value);
			set_busy(true);
			notice("Running " + tool + "…");
			const result = await api("/api/run", "POST", payload());
			output_text = result.output;
			controls.output.textContent = output_text;
			controls.copy.disabled = false;
			update_counts();
			notice("Done. " + output_text.length + " output characters.", "success");
		} catch (error) {
			clear_output();
			notice(error.message, "error");
		} finally { set_busy(false); }
	}

	async function save(as_new) {
		if (busy) return;
		if (!controls.title.value.trim()) { notice("Give this example a name first.", "error"); controls.title.focus(); return; }
		set_busy(true);
		try {
			const data = payload();
			if (!as_new && selected_id !== null) data.id = selected_id;
			const result = await api("/api/examples", "POST", data);
			selected_id = result.example.id;
			await refresh_examples();
			notice("Saved “" + result.example.title + "”.", "success");
		} catch (error) { notice(error.message, "error"); }
		finally { set_busy(false); }
	}

	controls.run.addEventListener("click", run);
	controls.save.addEventListener("click", () => save(false));
	controls["save-copy"].addEventListener("click", () => save(true));
	controls["new-example"].addEventListener("click", () => new_example(tool));
	controls.search.addEventListener("input", render_examples);
	controls.program.addEventListener("input", update_counts);
	controls.input.addEventListener("input", update_counts);
	for (const tab of document.querySelectorAll("[data-tool]")) {
		tab.addEventListener("click", () => { if (!busy && tool !== tab.dataset.tool) new_example(tab.dataset.tool); });
		tab.addEventListener("keydown", event => {
			if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
			event.preventDefault();
			const tools = ["sed", "m4", "mus"];
			const index = (tools.indexOf(tool) + (event.key === "ArrowRight" ? 1 : 2)) % 3;
			new_example(tools[index]);
			document.getElementById("tab-" + tools[index]).focus();
		});
	}
	document.addEventListener("keydown", event => {
		if ((event.ctrlKey || event.metaKey) && event.key === "Enter") { event.preventDefault(); run(); }
	});
	controls.copy.addEventListener("click", async () => {
		try { await navigator.clipboard.writeText(output_text); notice("Output copied.", "success"); }
		catch (_) { notice("Clipboard access is unavailable. Select the output and copy it manually.", "error"); }
	});
	controls.delete.addEventListener("click", async () => {
		if (busy || selected_id === null || !window.confirm("Delete this saved example?")) return;
		set_busy(true);
		try {
			await api("/api/examples?id=" + selected_id, "DELETE");
			await refresh_examples();
			new_example(tool);
			notice("Example deleted.", "success");
		} catch (error) { notice(error.message, "error"); }
		finally { set_busy(false); }
	});

	set_tool(tool);
	set_busy(true);
	notice("Loading saved examples…");
	refresh_examples().then(() => {
		const first = examples.find(item => item.tool === "sed");
		if (first) load_example(first);
		else notice("Choose an example or start with your own text.");
	}).catch(error => notice(error.message, "error")).finally(() => set_busy(false));
})();
