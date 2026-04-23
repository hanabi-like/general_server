var API = "/api/game-information-manager";

var LOGIN_URL = "/game-information-manager/login.html";

var LOGIN_KEY = "gameInformationManagerLogin";
var XUSERNAME_KEY = "gameInformationManagerXUserName";

var currentGameCharacterRegion = [];

function getGameCharacterUrl() {
    return API + "/game/" + getValue("gameName") + "/character";
}

function getGameCharacterRegionUrl() {
    return getGameCharacterUrl() + "/region/" + getValue("characterRegion");
}

function getItem(id) {
    return sessionStorage.getItem(id);
}

function getElement(id) {
    return document.getElementById(id);
}

function getValue(id) {
    return getElement(id).value;
}

function resetGameCharacterRegion() {
    getElement("GameCharacterRegion").innerHTML = "";
    getElement("GameCharacterRegionMessage").textContent = "";

    currentGameCharacterRegion = [];

    var currentRefName = getElement("characterRefName");
    currentRefName.innerHTML = "";

    var emptyOption = document.createElement("option");
    emptyOption.value = "";
    emptyOption.selected = true;
    emptyOption.disabled = true;
    emptyOption.textContent = "请选择";
    currentRefName.appendChild(emptyOption);

    getElement("characterInsertPos").value = "";
}

function formatGameCharacter(text) {
    getElement("GameCharacter").innerHTML = "";
    getElement("GameCharacterMessage").textContent = "";

    var data = null;

    try {
        data = JSON.parse(text);
    } catch (e) {
        getElement("GameCharacterMessage").textContent = "格式错误";
        return;
    }

    if (!data) {
        getElement("GameCharacterMessage").textContent = "暂无游戏";
        return;
    }

    var gameName = document.createElement("h3");
    gameName.textContent = data.gameName;
    getElement("GameCharacter").appendChild(gameName);

    if (!data.gameCharacterRegionResponseDTOList || data.gameCharacterRegionResponseDTOList.length === 0) {
        getElement("GameCharacterMessage").textContent = "游戏暂无角色列表";
        return;
    }

    data.gameCharacterRegionResponseDTOList.forEach(function (item) {
        var region = document.createElement("h4");
        region.textContent = item.region;
        getElement("GameCharacter").appendChild(region);

        if (!item.nameList || item.nameList.length === 0) {
            var empty = document.createElement("p");
            empty.textContent = "角色列表暂无角色";
            getElement("GameCharacter").appendChild(empty);
            return;
        }

        var nameList = document.createElement("p");
        nameList.textContent = item.nameList.join(" ");
        getElement("GameCharacter").appendChild(nameList);
    });
}

function formatGameCharacterRegion(text) {
    resetGameCharacterRegion();

    var currentRefName = getElement("characterRefName");

    var data = null;

    try {
        data = JSON.parse(text);
    } catch (e) {
        getElement("GameCharacterRegionMessage").textContent = "格式错误";
        return;
    }

    if (!data) {
        getElement("GameCharacterRegionMessage").textContent = "游戏暂无角色列表";
        return;
    }

    currentGameCharacterRegion = Array.isArray(data.nameList) ? data.nameList : [];

    if (!currentGameCharacterRegion || currentGameCharacterRegion.length === 0) {
        getElement("GameCharacterRegionMessage").textContent = "角色列表暂无角色";
        return;
    }

    var nameList = document.createElement("p");
    nameList.textContent = currentGameCharacterRegion.join(" ");
    getElement("GameCharacterRegion").appendChild(nameList);

    currentGameCharacterRegion.forEach(function (name) {
        var option = document.createElement("option");
        option.value = name;
        option.textContent = name;
        currentRefName.appendChild(option);
    });
}

function getGameCharacterForm() {
    return {
        name: getValue("characterName"),
        sex: getValue("characterSex"),
        region: getValue("characterRegion"),
        quality: getValue("characterQuality"),
        refName: getValue("characterRefName"),
        insertPos: getValue("characterInsertPos")
    };
}

function validateGameCharacterForm(form) {
    if (form.name === "" ||
        form.sex === "" ||
        form.region === "" ||
        form.quality === "") {
        getElement("message").textContent = "补充角色基本信息";
        return false;
    }

    if (currentGameCharacterRegion.length > 0 &&
        (form.refName === "" || form.insertPos === "")) {
        getElement("message").textContent = "补充参考角色和插入位置";
        return false;
    }

    return true;
}

function hasGameNameChanged() {
    if (getValue("characterRegion")) {
        onGetGameCharacterByRegion();
    }
}

function onGetGameCharacterByRegion() {
    resetGameCharacterRegion();

    if (!getItem(XUSERNAME_KEY)) {
        window.location.href = LOGIN_URL;
        return;
    }

    if (!getValue("gameName")) {
        getElement("GameCharacterRegionMessage").textContent = "进行游戏选择";
        return;
    }

    if (!getValue("characterRegion")) {
        getElement("GameCharacterRegionMessage").textContent = "进行归属选择";
        return;
    }

    getElement("GameCharacterRegionMessage").textContent = "查询中...";

    fetch(getGameCharacterRegionUrl(), {
        method: "GET",
        headers: {
            "X-User-Name": getItem(XUSERNAME_KEY)
        }
    }).then(function (response) {
        return response.text().then(function (text) {
            if (response.status >= 200 && response.status < 300) {
                formatGameCharacterRegion(text);
            } else {
                getElement("GameCharacterRegionMessage").textContent = "查询失败";
            }
        });
    }).catch(function () {
        getElement("GameCharacterRegionMessage").textContent = "服务暂不可用";
    });
}

function onGetGameCharacter(event) {
    event.preventDefault();

    getElement("GameCharacter").innerHTML = "";
    getElement("GameCharacterMessage").textContent = "";

    if (!getItem(XUSERNAME_KEY)) {
        window.location.href = LOGIN_URL;
        return;
    }

    if (!getValue("gameName")) {
        getElement("GameCharacterMessage").textContent = "进行游戏选择";
        return;
    }

    getElement("GameCharacterMessage").textContent = "查询中...";

    fetch(getGameCharacterUrl(), {
        method: "GET",
        headers: {
            "X-User-Name": getItem(XUSERNAME_KEY)
        }
    }).then(function (response) {
        return response.text().then(function (text) {
            if (response.status >= 200 && response.status < 300) {
                formatGameCharacter(text);
            } else {
                getElement("GameCharacterMessage").textContent = "查询失败";
            }
        });
    }).catch(function () {
        getElement("GameCharacterMessage").textContent = "服务暂不可用";
    });
}

function onSaveGameCharacter(event) {
    event.preventDefault();

    getElement("message").textContent = "";

    if (!getItem(XUSERNAME_KEY)) {
        window.location.href = LOGIN_URL;
        return;
    }

    if (!getValue("gameName")) {
        getElement("message").textContent = "进行游戏选择";
        return;
    }

    var form = getGameCharacterForm();
    if (!validateGameCharacterForm(form)) {
        return;
    }

    fetch(getGameCharacterUrl(), {
        method: "POST",
        headers: {
            "Content-Type": "application/json",
            "X-User-Name": getItem(XUSERNAME_KEY)
        },
        body: JSON.stringify({
            name: form.name,
            sex: Number(form.sex),
            region: form.region,
            quality: Number(form.quality),
            refName: form.refName,
            insertPos: form.insertPos
        })
    }).then(function (response) {
        return response.text().then(function () {
            if (response.status >= 200 && response.status < 300) {
                getElement("message").textContent = "添加成功";
                onGetGameCharacterByRegion();
            } else {
                getElement("message").textContent = "添加失败";
            }
        });
    }).catch(function () {
        getElement("message").textContent = "服务暂不可用";
    });
}

function getLoginStatus() {
    if (getItem(LOGIN_KEY) !== "true") {
        window.location.href = LOGIN_URL;
        return false;
    }

    return true;
}

function bindEvent() {
    getElement("gameName").addEventListener("change", hasGameNameChanged);
    getElement("characterRegion").addEventListener("change", onGetGameCharacterByRegion);
    getElement("getGameCharacter").addEventListener("submit", onGetGameCharacter);
    getElement("saveGameCharacter").addEventListener("submit", onSaveGameCharacter);
}

if (getLoginStatus()) {
    bindEvent();
}
